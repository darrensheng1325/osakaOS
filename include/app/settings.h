#ifndef __OS__APP__SETTINGS_H
#define __OS__APP__SETTINGS_H

#include <app.h>
#include <common/types.h>
#include <gui/widget.h>
#include <filesys/ofs.h>
#include <drivers/vga.h>
#include <stdint.h>

namespace os {
	namespace gui {
		class CompositeWidget;
	}
	namespace filesystem {
		class FileSystem;
	}

	class Settings : public App {
		public:
			Settings();
			~Settings();

			void ComputeAppState(common::GraphicsContext* gc, gui::CompositeWidget* widget);
			void DrawAppMenu(common::GraphicsContext* gc, gui::CompositeWidget* widget);
			void OnMouseDown(common::int32_t x, common::int32_t y, common::uint8_t button, gui::CompositeWidget* widget);
			void OnKeyDown(char ch, gui::CompositeWidget* widget);
			
			void SaveOutput(char* fileName, gui::CompositeWidget* widget, filesystem::FileSystem* filesystem);
			void ReadInput(char* fileName, gui::CompositeWidget* widget, filesystem::FileSystem* filesystem);
			
		private:
			bool init;
			uint8_t selectedCategory; // 0=Desktop, 1=Windows, 2=Iframes
			uint8_t selectedItem; // Which item in the category
			
			// Settings values
			uint8_t desktopBgColor;
			uint8_t windowTopBarColor;
			uint8_t iframeDisplayMode; // 0=canvas capture, 1=direct overlay
			char iframeProxy[256];
			
			// UI element positions (for click detection)
			int32_t saveButtonX;
			int32_t saveButtonY;
			
			void DrawSettingsUI(common::GraphicsContext* gc, gui::CompositeWidget* widget);
			void LoadSettings();
			void SaveSettings();
			void ApplySettings();
	};
}

#endif

