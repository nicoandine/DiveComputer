#ifndef LOG_INFO_GUI_HPP
#define LOG_INFO_GUI_HPP

#include "qtheaders.hpp"
#include "log_info.hpp"
#include "global.hpp"

namespace DiveComputer {

class LogViewerWindow : public QMainWindow {
    Q_OBJECT

public:
    LogViewerWindow(QWidget *parent = nullptr);
    ~LogViewerWindow();

signals:
    void windowClosed();

private:
    // Window size
    const int WindowWidth = 800;
    const int WindowHeight = 600;
    
    // UI components
    QTextEdit* m_logTextEdit;
    QPushButton* m_refreshButton;
    QPushButton* m_downloadButton;
    
    // Load log content
    void loadLogContent();
    void closeEvent(QCloseEvent* event) override;

private slots:
    void refreshLog();
    void downloadLog();
};

} // namespace DiveComputer

#endif // LOG_INFO_GUI_HPP