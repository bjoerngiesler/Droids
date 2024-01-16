#if !defined(RMENU_H)
#define RMENU_H

#include <vector>
#include <functional>
#include <LibBB.h>

#include "Config.h"
#include "RDisplay.h"
#include "RemoteInput.h"

using namespace bb;

class RMenu: public RDrawable, public RemoteInput::Delegate {
public:
  RMenu(const char* title);

  void addEntry(const char* title, std::function<void(void)> callback);
  void clear();

  Result draw(ConsoleStream* stream = NULL);
  void buttonTopLeftPressed();
  void buttonTopRightPressed();
  void buttonConfirmPressed();
  void buttonTopLeftReleased();
  void buttonTopRightReleased();
  void buttonConfirmReleased();

protected:
  struct Entry {
    const char* title;
    std::function<void(void)> callback;
  };

  const char* title_;
  std::vector<Entry> entries_;
  uint8_t numEntries_;
  uint8_t cursor_;
};


#endif