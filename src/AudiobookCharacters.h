/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct MainWindow;

// The Audiobook Characters panel: docked on the left of the frame, beside the
// document, with a splitter. Not a window of its own.
void ToggleAudiobookPanel(MainWindow* win);
void DestroyAudiobookPanel(MainWindow* win);
bool IsAudiobookPanelVisible(MainWindow* win);

// called from SumatraPDF.cpp after the frame moves/resizes the panel
void RelayoutAudiobookPanel(MainWindow* win);

// implemented in SumatraPDF.cpp (needs the Chatterbox install path)
void AudiobookTrainVoice(Str character, Str bookPath);

// Start the engine idle (TTS model loaded, nothing read) if it isn't already
// up, so the panel has a cast to show. Implemented in SumatraPDF.cpp.
bool AudiobookEnsureEngineForCurrentTab();

// The Chatterbox engine is a separate process; the playback bar drives it
// through these. Paths are the control API's: /pause /resume /stop /restart
// /prev /next /page
bool AudiobookIsRunning();
bool AudiobookSendCommand(Str path, Str jsonBody = {});
