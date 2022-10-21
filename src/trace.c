#include "trace.h"
#include "heap.h"
#include "queue.h"
#include "mutex.h"
#include "timer_object.h"

#define WIN32_LEAN_AND_MEAN
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

// The event struct that stores information for an event
typedef struct event_t
{
	heap_t* heap;
	char* name;
	char ph;
	int pid;
	DWORD tid;
} event_t;

// The trace struct that stores information for a trace
typedef struct trace_t
{
	heap_t* heap;
	queue_t* queue;
	mutex_t* mutex;
	size_t event_capacity;
	timer_object_t* timer;
	char* path;
	char* buffer;
	bool capture;
} trace_t;

trace_t* trace_create(heap_t* heap, int event_capacity)
{
	trace_t* trace = heap_alloc(heap, sizeof(trace_t), 8);
	trace->heap = heap;
	trace->queue = queue_create(heap, event_capacity);
	trace->mutex = mutex_create(heap);
	trace->event_capacity = (size_t)event_capacity;
	trace->timer = timer_object_create(heap, NULL);
	trace->buffer = calloc(trace->event_capacity * 256, sizeof(char));
	trace->capture = false;
	return trace;
}

void trace_destroy(trace_t* trace)
{
	queue_push(trace->queue, NULL);
	queue_destroy(trace->queue);
	timer_object_destroy(trace->timer);
	mutex_destroy(trace->mutex);
	free(trace->buffer);
	heap_free(trace->heap, trace);
}

void trace_duration_push(trace_t* trace, const char* name)
{
	timer_object_update(trace->timer);
	// Create a start event with corresponding name
	event_t* event = heap_alloc(trace->heap, sizeof(event_t), 8);
	event->heap = trace->heap;
	event->name = calloc(strlen(name) + 1, sizeof(char));
	event->ph = 'B';
	event->pid = 0;
	strncpy_s(event->name, strlen(name)+1, name, strlen(name));
	event->tid = GetCurrentThreadId();
	queue_push(trace->queue, event);
	
	if (trace->capture) 
	{
		char event_string[128];
		snprintf(event_string, sizeof(event_string), "\t\t{\"name\":\"%s\",\"ph\":\"%c\",\"pid\":%d,\"tid\":\"%d\",\"ts\":\"%d\"},\n",
			event->name, event->ph, event->pid, event->tid, timer_object_get_ms(trace->timer));
		
		mutex_lock(trace->mutex);
		strncat_s(trace->buffer, strlen(trace->buffer) + strlen(event_string) + 1, event_string, strlen(event_string));
		mutex_unlock(trace->mutex);
	}
}

void trace_duration_pop(trace_t* trace)
{
	timer_object_update(trace->timer);

	// Get the poped event
	event_t* event = queue_pop(trace->queue);
	event->ph = 'E';

	if(trace->capture)
	{
		char event_string[128];
		snprintf(event_string, sizeof(event_string), "\t\t{\"name\":\"%s\",\"ph\":\"%c\",\"pid\":%d,\"tid\":\"%d\",\"ts\":\"%d\"},\n",
			event->name, event->ph, event->pid, event->tid, timer_object_get_ms(trace->timer));
		
		mutex_lock(trace->mutex);
		strncat_s(trace->buffer, trace->event_capacity * 256, event_string, strlen(event_string));
		mutex_unlock(trace->mutex);
	}

	free(event->name);
	heap_free(event->heap, event);
}

void trace_capture_start(trace_t* trace, const char* path)
{
	trace->path = calloc(strlen(path) + 1, sizeof(char));
	strncpy_s(trace->path, strlen(path)+1, path, strlen(path));
	char* start_string = "{\n\t\"displayTimeUnit\": \"ns\", \"traceEvents\": [\n";
	mutex_lock(trace->mutex);
	strncat_s(trace->buffer, strlen(trace->buffer) + strlen(start_string) + 1, start_string, strlen(start_string));
	mutex_unlock(trace->mutex);
	trace->capture = true;
}

void trace_capture_stop(trace_t* trace)
{
	trace->capture = false;
	char* end_string = "\t]\n";
	mutex_lock(trace->mutex);
	strncat_s(trace->buffer, strlen(trace->buffer) + strlen(end_string) + 1, end_string, strlen(end_string));

	// The same logic as file_write() from fs.c
	wchar_t wide_path[1024];
	if (MultiByteToWideChar(CP_UTF8, 0, trace->path, -1, wide_path, sizeof(wide_path)) <= 0)
	{
		perror("Invalid file path");
		return;
	}

	HANDLE handle = CreateFile(wide_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		perror("Failed to create the trace file");
		return;
	}

	DWORD bytes_written = 0;
	if (!WriteFile(handle, trace->buffer, (DWORD)strlen(trace->buffer), NULL, NULL))
	{
		perror("Failed to wirte the trace file");
		return;
	}
																											
	CloseHandle(handle);
	mutex_unlock(trace->mutex);
}
