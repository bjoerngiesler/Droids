#if !defined(RGRAPHS_H)
#define RGRAPHS_H

#include <LibBB.h>

#include "RDisplay.h"
#include "RemoteInput.h"

using namespace bb;

class RGraphs: public RDrawable, public RemoteInput::Delegate {
public:
  enum Graph {
    TOP     = 0,
    MIDDLE  = 1,
    BOTTOM  = 2
  };

  static const int GRAPH_WIDTH = 60;
  static const int GRAPH_HEIGHT = 50;
  static const int GRAPH_X = 18;
  static const int GRAPH0_Y = 3;
  static const int GRAPH_DIST = 3;

  RGraphs();
  virtual Result draw(ConsoleStream* stream = NULL);
  void setTitle(Graph g, const char* t);
  void plotAxisData(Graph g, float a0, float a1, float a2, float a3, float a4);
  void plotControlPacket(Graph g, const bb::ControlPacket& packet);
  void advanceCursor(Graph g);

  void buttonTopLeftPressed();
  void buttonTopRightPressed();
  void buttonConfirmPressed();

protected:
  const char *title_[3];
  uint8_t cursor_[3];
};
#endif