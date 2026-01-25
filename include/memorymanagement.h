#ifndef __OS__MEMORYMANAGEMENT_H
#define __OS__MEMORYMANAGEMENT_H

#include <common/types.h>
#include <stddef.h>



namespace os {

	struct MemoryChunk {

		MemoryChunk* next;
		MemoryChunk* prev;
		bool allocated;
		common::size_t size;
	};

	class MemoryManager {
	
		public:
		//protected:
			MemoryChunk* first;
			common::uint32_t size = 0;
		
		public:
			static MemoryManager* activeMemoryManager;

			MemoryManager(common::size_t start, common::size_t size);
			~MemoryManager();

			void* malloc(common::size_t size);
			void free(void* ptr);

			void BufferShift(common::uint8_t* buffer, common::uint32_t bufsize, 
					 common::uint32_t index, common::int32_t shift);
	};
}


void* operator new(size_t size);
void* operator new[](size_t size);

// Note: placement new is provided by the standard library, no need to redeclare

void operator delete(void* ptr);
void operator delete[](void* ptr);



#endif
