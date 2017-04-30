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

    DocumentBg,
    DocumentText,

    // text color of regular notification
    NotificationsBg,
    NotificationsText,

    // text/background color of highlighted notfications
    NotificationsHighlightBg,
    NotificationsHighlightText,

    NotifcationsProgress,

    Link,
};

COLORREF GetAppColor(AppColor);
void GetFixedPageUiColors(COLORREF& text, COLORREF& bg);
void GetEbookUiColors(COLORREF& text, COLORREF& bg);
