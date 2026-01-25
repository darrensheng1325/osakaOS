#include <memorymanagement.h>
#include <stddef.h>
#include <stdlib.h>


using namespace os;
using namespace os::common;



MemoryManager* MemoryManager::activeMemoryManager = 0;

MemoryManager::MemoryManager(os::common::size_t start, os::common::size_t size) {

	activeMemoryManager = this;

	if (size < sizeof(MemoryChunk)) {
		
		first = 0;
	} else {
		first = (MemoryChunk*)start;
	
		first -> allocated = false;
		first -> prev = 0;
		first -> next = 0;
		first -> size = size - sizeof(MemoryChunk);
	}
}



MemoryManager::~MemoryManager() {

	if (activeMemoryManager == this) {
	
		activeMemoryManager = 0;
	}
}



void* MemoryManager::malloc(os::common::size_t size) {

	this->size += (uint32_t)size;

	MemoryChunk *result = 0;

	for (MemoryChunk* chunk = first; chunk != 0 && result == 0; chunk = chunk->next) {
	
		if (chunk->size > size && !chunk->allocated) {
		
			result = chunk;
		}
	}
	if (result == 0) { return 0; }

	if (result->size >= size + sizeof(MemoryChunk) + 1) {
	
		MemoryChunk* temp = (MemoryChunk*)((os::common::size_t)result + sizeof(MemoryChunk) + size);

		temp->allocated = false;
		temp->size = result->size - size - sizeof(MemoryChunk);
		temp->prev = result;
		temp->next = result->next;

		if (temp->next != 0) {
		
			temp->next->prev = temp;
		}
		result->size = size;
		result->next = temp;
	}
	
	result->allocated = true;
	return (void*)(((os::common::size_t)result) + sizeof(MemoryChunk));
}



void MemoryManager::free(void* ptr) {

	MemoryChunk* chunk = (MemoryChunk*)((os::common::size_t)ptr - sizeof(MemoryChunk));
	chunk -> allocated = false;
	
	if (chunk->prev != 0 && !chunk->prev->allocated) {
	
		chunk->prev->next = chunk->next;
		chunk->prev->size += chunk->size + sizeof(MemoryChunk);

		if (chunk->next != 0) {
		
			chunk->next->prev = chunk->prev;
		}

		chunk = chunk->prev;
	}

	if (chunk->next != 0 && !chunk->next->allocated) {
	
		chunk->size += chunk->next->size + sizeof(MemoryChunk);
		chunk->next = chunk->next->next;

		if (chunk->next != 0) {
		
			chunk->next->prev = chunk;
		}
	}
}


//shift values in buffer starting from index
void MemoryManager::BufferShift(uint8_t* buffer, uint32_t bufsize, 
				uint32_t index, int32_t shift) {

	for (uint32_t i = index; i < bufsize; i++) {
	
		buffer[index+shift] = buffer[index];
	}
}



void* operator new(::size_t size) {

	if (os::MemoryManager::activeMemoryManager == 0) {
	
		return 0;
	}
	return os::MemoryManager::activeMemoryManager->malloc(size);
}



void* operator new[](::size_t size) {

	if (os::MemoryManager::activeMemoryManager == 0) {
	
		return 0;
	}
	return os::MemoryManager::activeMemoryManager->malloc(size);
}



// Note: placement new is provided by the standard library

void operator delete(void* ptr) {

	if (os::MemoryManager::activeMemoryManager != 0) {
	
		os::MemoryManager::activeMemoryManager->free(ptr);
	}
}



void operator delete[](void* ptr) {
	
	if (os::MemoryManager::activeMemoryManager != 0) {
	
		os::MemoryManager::activeMemoryManager->free(ptr);
	}
}



