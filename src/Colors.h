// application-wide colors

enum class AppColor {
    // background color of the main window
    NoDocBg,
    // background color of about window
    AboutBg,
    // background color of log
    LogoBg,

    MainWindowBg,
    MainWindowText,
    MainWindowLink,

    DocumentBg,
    DocumentText,

    // text color of regular notification
    NotificationsBg,
    NotificationsText,

    // text/background color of highlighted notfications
    NotificationsHighlightBg,
    NotificationsHighlightText,

    NotifcationsProgress,

    TabSelectedBg,
    TabSelectedText,
    TabSelectedCloseX,
    TabSelectedCloseCircle,

    TabBackgroundBg,
    TabBackgroundText,
    TabBackgroundCloseX,
    TabBackgroundCloseCircle,

    TabHighlightedBg,
    TabHighlightedText,
    TabHighlightedCloseX,
    TabHighlightedCloseCircle,

    TabHoveredCloseX,
    TabHoveredCloseCircle,

    TabClickedCloseX,
    TabClickedCloseCircle,

};

COLORREF AdjustLightness(COLORREF c, float factor);
COLORREF AdjustLightness2(COLORREF c, float units);
float GetLightness(COLORREF c);

COLORREF GetAppColor(AppColor);
void GetFixedPageUiColors(COLORREF& text, COLORREF& bg);
void GetEbookUiColors(COLORREF& text, COLORREF& bg);
