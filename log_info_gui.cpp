#include "log_info_gui.hpp"

namespace DiveComputer {

LogViewerWindow::LogViewerWindow(QWidget *parent) : QMainWindow(parent) {
    // Set window title
    setWindowTitle("Log Viewer");
    
    // Configure window size and position
    setWindowSizeAndPosition(this, WindowWidth, WindowHeight, WindowPosition::CENTER);
    
    // Enable attribute to detect when window is closed
    setAttribute(Qt::WA_DeleteOnClose);
    
    // Create central widget
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    
    // Create button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    // Create buttons
    m_refreshButton = new QPushButton("Refresh", this);
    m_downloadButton = new QPushButton("Download", this);
    
    // Add buttons to layout
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addWidget(m_downloadButton);
    buttonLayout->addStretch(); // Push buttons to the left
    
    // Add button layout to main layout
    mainLayout->addLayout(buttonLayout);
    
    // Create text edit for log display
    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setReadOnly(true); // Make it read-only
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(10);
    m_logTextEdit->setFont(monoFont);
    
    // Add text edit to main layout
    mainLayout->addWidget(m_logTextEdit);
    
    // Connect signals and slots
    connect(m_refreshButton, &QPushButton::clicked, this, &LogViewerWindow::refreshLog);
    connect(m_downloadButton, &QPushButton::clicked, this, &LogViewerWindow::downloadLog);
    
    // Load log content initially
    loadLogContent();
}

LogViewerWindow::~LogViewerWindow() {
    // No need for explicit cleanup
    // Emit signal to inform parent the window is being destroyed
    emit windowClosed();
}

void LogViewerWindow::loadLogContent() {
    // Get the log file path
    std::string logFilePath = getFilePath(LOG_FILE_NAME);
    
    // Clear the text edit
    m_logTextEdit->clear();
    
    // Try to open and read the log file
    QFile logFile(QString::fromStdString(logFilePath));
    if (logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Read all content
        QTextStream in(&logFile);
        QString logContent = in.readAll();
        
        // Set the content to the text edit
        m_logTextEdit->setPlainText(logContent);
        
        // Scroll to the bottom to show most recent entries
        QTextCursor cursor = m_logTextEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_logTextEdit->setTextCursor(cursor);
        
        // Close the file
        logFile.close();
        
        // Log the action
        logWrite("Log file loaded for viewing");
    } else {
        // Display error message if file couldn't be opened
        m_logTextEdit->setPlainText("Error: Could not open log file at " + 
                                   QString::fromStdString(logFilePath));
        
        // Log the error
        logWrite("Error opening log file for viewing: ", logFilePath);
    }
}

void LogViewerWindow::refreshLog() {
    // Simply reload the log content
    loadLogContent();
    
    // Log the action
    logWrite("Log view refreshed");
}

void LogViewerWindow::downloadLog() {
    // Open file dialog to get save location
    QString saveFilePath = QFileDialog::getSaveFileName(
        this,
        "Save Log File",
        QDir::homePath() + "/divelog.txt",
        "Text Files (*.txt);;All Files (*)"
    );
    
    // If user didn't cancel
    if (!saveFilePath.isEmpty()) {
        // Get the source log file path
        std::string logFilePath = getFilePath(LOG_FILE_NAME);
        
        // Try to copy the file
        QFile sourceFile(QString::fromStdString(logFilePath));
        if (sourceFile.exists()) {
            // If a file already exists at the destination, remove it
            QFile destFile(saveFilePath);
            if (destFile.exists()) {
                destFile.remove();
            }
            
            // Copy the file
            if (sourceFile.copy(saveFilePath)) {
                // Show success message
                QMessageBox::information(
                    this,
                    "Success",
                    "Log file saved successfully to:\n" + saveFilePath
                );
                
                // Log the action
                logWrite("Log file downloaded/saved to: ", saveFilePath.toStdString());
            } else {
                // Show error message
                QMessageBox::critical(
                    this,
                    "Error",
                    "Failed to save log file:\n" + sourceFile.errorString()
                );
                
                // Log the error
                logWrite("Error saving log file: ", sourceFile.errorString().toStdString());
            }
        } else {
            // Show error message
            QMessageBox::critical(
                this,
                "Error",
                "Source log file does not exist."
            );
            
            // Log the error
            logWrite("Error: Source log file does not exist at ", logFilePath);
        }
    }
}

void LogViewerWindow::closeEvent(QCloseEvent* event) {
    // Handle the close event
    QMainWindow::closeEvent(event);
}

} // namespace DiveComputer