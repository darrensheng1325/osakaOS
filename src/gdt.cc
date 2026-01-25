#include <gdt.h>

using namespace os;
using namespace os::common;


GlobalDescriptorTable::GlobalDescriptorTable()
#ifdef __EMSCRIPTEN__
// For Emscripten, GDT is not needed (no x86 segmentation)
// Just initialize members to zero/default values
: nullSegmentSelector(0, 0, 0),
unusedSegmentSelector(0, 0, 0),
codeSegmentSelector(0, 0, 0),
dataSegmentSelector(0, 0, 0) {
	// No-op for web builds
}
#else
: nullSegmentSelector(0, 0, 0),
unusedSegmentSelector(0, 0, 0),
codeSegmentSelector(0, 64*1024*1024, 0x9A),
dataSegmentSelector(0, 64*1024*1024, 0x92) {

	uint32_t i[2];
	i[0] = (uint32_t)this;
	i[1] = sizeof(GlobalDescriptorTable) << 16;

	asm volatile("lgdt (%0)": :"p" (((uint8_t *) i) + 2));
}
#endif



GlobalDescriptorTable::~GlobalDescriptorTable() {
}



uint16_t GlobalDescriptorTable::DataSegmentSelector() {
#ifdef __EMSCRIPTEN__
	// Return a dummy value for web builds
	return 0x10;
#else
	return (uint8_t*)&dataSegmentSelector - (uint8_t*)this;
#endif
}



uint16_t GlobalDescriptorTable::CodeSegmentSelector() {
#ifdef __EMSCRIPTEN__
	// Return a dummy value for web builds
	return 0x08;
#else
	return (uint8_t*)&codeSegmentSelector - (uint8_t*)this;
#endif
}



GlobalDescriptorTable::SegmentDescriptor::SegmentDescriptor(uint32_t base, uint32_t limit, uint8_t flags) {

#ifdef __EMSCRIPTEN__
	// For Emscripten, use member access instead of pointer arithmetic
	// to avoid memory validation issues
	limit_lo = limit & 0xFFFF;
	base_lo = base & 0xFFFF;
	base_hi = (base >> 16) & 0xFF;
	type = flags;
	
	if (limit <= 65536) {
		flags_limit_hi = 0x40 | ((limit >> 16) & 0xF);
	} else {
		if ((limit & 0xFFF) != 0xFFF) {
			limit = (limit >> 12) - 1;
		} else {
			limit = limit >> 12;
		}
		flags_limit_hi = 0xC0 | ((limit >> 16) & 0xF);
		limit_lo = limit & 0xFFFF;
	}
	
	base_vhi = (base >> 24) & 0xFF;
#else
	uint8_t* target = (uint8_t*)this;

	if (limit <= 65536)  {
	
		target[6] = 0x40;
	} else {

		if ((limit & 0xFFF) != 0xFFF) {

			limit = (limit >> 12) - 1;
		} else {

			limit = limit >> 12;
		}

		target[6] = 0xC0;
	}

	target[0] = limit & 0xFF;
	target[1] = (limit >> 8) & 0xFF;
	target[6] |= (limit >> 16) & 0xF;

	target[2] = base & 0xFF;
	target[3] = (base >> 8) & 0xFF;
	target[4] = (base >> 16) & 0xFF;
	target[7] = (base >> 24) & 0xFF;

	target[5] = flags;
#endif
}




uint32_t GlobalDescriptorTable::SegmentDescriptor::Base() {

	uint8_t* target = (uint8_t*)this;
	uint32_t result = target[7];
	
	result = (result << 8) + target[4];
	result = (result << 8) + target[3];
	result = (result << 8) + target[2];
	
	return result;
}



uint32_t GlobalDescriptorTable::SegmentDescriptor::Limit() {


	uint8_t* target = (uint8_t*)this;
	uint32_t result = target[6] & 0xF;

	result = (result << 8) + target[1];
	result = (result << 8) + target[0];

	if ((target[6] & 0xC0) == 0xC0) {
		result = (result << 12) | 0xFFF;
	}

	return result;
}


