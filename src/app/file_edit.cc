#include <app/file_edit.h>
#include <new>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
extern "C" void printf(char*);
#else
void printf(char*);
#endif

// Forward declarations
bool strcmp(char* str1, char* str2);

using namespace os;
using namespace os::gui;
using namespace os::common;
using namespace os::filesystem;


Journal::Journal() {

	this->appType = 3;

	//init block
	for (uint16_t i = 0; i < OFS_BLOCK_SIZE; i++) {
	
		this->LBA[i] = 0x00;
		this->LBA2[i] = 0x00;
	}
}

Journal::~Journal() {
}


void Journal::ComputeAppState(GraphicsContext* gc, CompositeWidget* widget) {

	if (!init) {
	
		this->DrawTheme(widget);
		// Enable menu to show tabs
		widget->Menu = true;
		init = true;
	}
	
	// Always draw tabs (they're always visible, not toggleable)
	if (widget->Menu) {
		this->DrawAppMenu(gc, widget);
	}
	
	App::ComputeAppState(gc, widget);
}


void Journal::DrawTheme(CompositeWidget* widget) {

	//blue lines (start below tabs)
	for (uint8_t y = font_height-1 + 12; y < 200; y += font_height) {
		
		widget->DrawLine(widget->x+0, widget->y+y, widget->x+320, widget->y+y, 0x2b);
	}
	
	//red line
	widget->DrawLine(widget->x+0, widget->y+0, widget->x+0, widget->y+200, 0x3c);
	
	// Draw separator line below tabs
	uint8_t offsetY = 10 * !widget->Fullscreen;
	widget->DrawLine(widget->x+0, widget->y+offsetY+12, widget->x+320, widget->y+offsetY+12, 0x2b);
}



void Journal::DrawAppMenu(GraphicsContext* gc, CompositeWidget* widget) {
	// Draw tabs/buttons at the top of the journal content area for save/open
	// These work even when browser overrides keyboard shortcuts
	uint16_t offsetx = 1 * !widget->Fullscreen;
	uint8_t offsety = 10 * !widget->Fullscreen;
	
	int32_t tabX = widget->x + offsetx + 1;
	int32_t tabY = widget->y + offsety; // Start of content area
	
	// Save button (40px wide, 12px tall)
	// gc->FillRectangle(tabX, tabY, 40, 12, 0x07); // Background (x, y, width, height)
	// gc->DrawRectangle(tabX, tabY, 40, 12, 0x3f); // Border (x, y, width, height)
	// gc->PutText("Save", tabX + 2, tabY + 2, 0x3f);
	
	// Open button (40px wide, 12px tall)
	// gc->FillRectangle(tabX + 42, tabY, 40, 12, 0x07); // Background (x, y, width, height)
	// gc->DrawRectangle(tabX + 42, tabY, 40, 12, 0x3f); // Border (x, y, width, height)
	// gc->PutText("Open", tabX + 44, tabY + 2, 0x3f);
}




void Journal::SaveOutput(char* fileName, CompositeWidget* widget, FileSystem* filesystem) {

	// Validate filename is not empty
	if (fileName == nullptr || fileName[0] == '\0') {
		printf("Cannot save: filename is empty\n");
		return;
	}

	// Check if file exists on disk
	uint32_t fileSector = filesystem->GetFileSector(fileName);
	bool fileExistsOnDisk = filesystem->FileIf(fileSector);
	
	// Check if file exists in in-memory table
	bool fileExistsInTable = false;
	for (int i = 0; i < filesystem->table->fileCount; i++) {
		File* file = (File*)(filesystem->table->files->Read(i));
		if (file && strcmp(fileName, file->Name)) {
			fileExistsInTable = true;
			break;
		}
	}
	
	if (fileExistsOnDisk) {
		// File exists on disk, write to it
		if (!filesystem->WriteLBA(fileName, LBA, 0)) {
			printf("Failed to write to existing file\n");
			return;
		}
		
		// If file exists on disk but not in table, add it to table
		if (!fileExistsInTable) {
			File* newFile = (File*)(filesystem->memoryManager->malloc(sizeof(File)));
			new (newFile) File(fileSector, OFS_BLOCK_SIZE, fileName);
			filesystem->table->files->Push(newFile);
			filesystem->table->fileCount++;
		}
	} else {
		// File doesn't exist, create new one
		if (!filesystem->NewFile(fileName, LBA, OFS_BLOCK_SIZE)) {
			printf("Failed to create new file\n");
			return;
		}
	}
	
	// Note: IndexedDB writes are asynchronous but transactional.
	// They will complete in the background and persist correctly.
	// We don't need to wait for them here as Emscripten doesn't allow
	// nested async operations (emscripten_sleep would conflict with IndexedDB).
	
	printf("File saved successfully\n");
}


void Journal::ReadInput(char* fileName, CompositeWidget* widget, FileSystem* filesystem) {

	if (filesystem->FileIf(filesystem->GetFileSector(fileName))) {
	
		filesystem->ReadLBA(fileName, LBA, 0);
	} else {
		//fill in with empty zeros
		for (uint16_t i = 0; i < OFS_BLOCK_SIZE; i++) { LBA[i] = 0x00; }
		
		//write little error message
		const char* error = "file not found";
		for (uint8_t i = 0; error[i] != '\0'; i++) { LBA[i] = error[i]; }
	}
	widget->Print("\v");
	this->DrawTheme(widget);
	// Tabs will be redrawn by ComputeAppState on next frame
	widget->textColor = 0x40;
	for (index = 0; LBA[index] != 0x00; index++) {
		
		widget->PutChar((char)(LBA[index]));
	}
	widget->PutChar('\v');
	widget->textColor = 0x3f;
}



void Journal::OnKeyDown(char ch, CompositeWidget* widget) {
	
	widget->Print("\v");
	this->DrawTheme(widget);
	// Tabs will be redrawn by ComputeAppState on next frame
	widget->textColor = 0x40;


	//change to next block
	//looks like thats not finished lol
	if (index >= OFS_BLOCK_SIZE) {
	
		return;
	}


	int i = 0;

	//key input
	switch (ch) {
	
		case '\b':
			if (index > 0 && cursor > 0) {
			
				if (cursor < index) {

					for (i = cursor-1; i < index; i++) {
				
						LBA[i] = LBA[i+1];
					}
				}
				index--;
				cursor--;
				LBA[index] = 0x00;
			}
			break;
		//left
		case '\xfc':
			cursor -= 1 * (cursor > 0);
			break;
		//up
		case '\xfd':
			while(cursor > 0 && LBA[cursor] != '\n') { cursor--; }
			cursor -= 1 * (cursor > 0);
			break;
		//down
		case '\xfe':
			while(cursor < index && LBA[cursor] != '\n') { cursor++; }
			cursor += 1 * (cursor < index);
			break;
		//right
		case '\xff':
			cursor += 1 * (cursor < index);
			break;
		default:
			if (cursor < index) {

				LBA[index+1] = LBA[index];
				
				for (i = index; i > cursor; i--) {
				
					LBA[i] = LBA[i-1];
				}
			}
			LBA[cursor] = (uint8_t)(ch);
			index++;
			cursor++;
			break;
	}


	//determine where text ends
	uint16_t prevNewLine = 0;
	uint16_t j = 0;
	bool cursorPassed = false;

	for (j = 0; j < index; j++) {
	
		if (LBA[j] == '\n') {
		
			prevNewLine = j;
			if (cursorPassed) { break; }
		}
		cursorPassed = (j >= cursor);
	}


	//limit how much text gets printed
	//(smoother scrolling)
	uint16_t printLimit = index;
	

	//print text to screen	
	for (i = 0; i < printLimit; i++) {
	
		if (i == cursor) {
	
			widget->PutChar('\v');
			if (LBA[i] == '\n') { widget->PutChar('\n'); }
		} else {
			if (LBA[i] == '\v') {
			
				widget->PutChar(' ');
				widget->PutChar(' ');
				widget->PutChar(' ');
				widget->PutChar(' ');
			} else {
				widget->PutChar((char)(LBA[i]));
			}
		}

		//dont scroll screen if cursor aint there
		if (widget->textScroll) { printLimit = j; }
	}
	if (cursor == index) { widget->PutChar('\v'); }

	
	widget->textColor = 0x3f;
}

void Journal::OnKeyUp(char ch, CompositeWidget* widget) {
}



void Journal::OnMouseDown(int32_t x, int32_t y, uint8_t button, CompositeWidget* widget) {

	// Check if clicking on tabs (relative to widget)
	uint16_t offsetx = 1 * !widget->Fullscreen;
	uint8_t offsety = 10 * !widget->Fullscreen;
	
	// Calculate relative position in content area
	int32_t relX = x - widget->x - offsetx;
	int32_t relY = y - widget->y - offsety;
	
	// Check if clicking on Save button (x: 1-41, y: 0-12)
	if (relX >= 1 && relX < 41 && relY >= 0 && relY < 12 && button == 1) {
		// Open save file dialog
		Window* win = (Window*)widget;
		win->FileWindow = true;
		win->Save = true;
		// Clear filename buffer
		for (int i = 0; i < 32; i++) {
			win->fileName[i] = '\0';
		}
		win->fileNameIndex = 0;
		return;
	}
	
	// Check if clicking on Open button (x: 42-82, y: 0-12)
	if (relX >= 42 && relX < 82 && relY >= 0 && relY < 12 && button == 1) {
		// Open read file dialog
		Window* win = (Window*)widget;
		win->FileWindow = true;
		win->Save = false;
		// Clear filename buffer
		for (int i = 0; i < 32; i++) {
			win->fileName[i] = '\0';
		}
		win->fileNameIndex = 0;
		return;
	}

	widget->Dragging = true;
}
void Journal::OnMouseUp(int32_t x, int32_t y, uint8_t button, CompositeWidget* widget) {
}
void Journal::OnMouseMove(int32_t oldx, int32_t oldy, 
			int32_t newx, int32_t newy, 
			CompositeWidget* widget) {
}
