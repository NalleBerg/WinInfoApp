#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QKeySequence>
#include <QCloseEvent>
#include <QIcon>
#include <QStyle>
#include <QPixmap>
#include <QPainter>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QTextBrowser>
#include <QShortcut>

const char* versionnumber = "0.1.0";

// --- Helper Functions ---
QIcon getAppIcon() {
    QIcon icon("wininfoapp.png");
    if (!icon.isNull()) return icon;
    icon = QIcon("wininfoapp.ico");
    return icon;
}

QIcon makeQuestionIcon() {
    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor("#0057b7"));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(0, 0, 32, 32, 8, 8);
    p.setPen(QPen(Qt::white, 4));
    p.setFont(QFont("Arial", 20, QFont::Bold));
    p.drawText(pix.rect(), Qt::AlignCenter, "?");
    return QIcon(pix);
}

QIcon makePinIcon(const QColor &color) {
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 2);
    p.setPen(pen);
    p.setBrush(color);
    p.drawEllipse(4, 2, 8, 8);
    p.drawRect(7, 10, 2, 4);
    return QIcon(pix);
}

inline void addCtrlWClose(QDialog *dlg) {
    QShortcut *closeShortcut = new QShortcut(QKeySequence("Ctrl+W"), dlg);
    closeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(closeShortcut, &QShortcut::activated, dlg, &QDialog::accept);
}

// --- MainWindow Class ---
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
protected:
    void closeEvent(QCloseEvent *event) override;
private slots:
    void setAlwaysOnTop(bool on);
    void tryQuit();
    void showAbout();
    void handleCtrlW();
private:
    bool confirmQuit();
    QAction *alwaysOnTopAct;
    QAction *exitAct;
    QShortcut *ctrlWShortcut;
    bool quitting = false;
};

MainWindow::MainWindow() {
    setWindowTitle("WinInfoApp");
    setWindowIcon(getAppIcon());

    QMenu *infoMenu = menuBar()->addMenu("&Information");

    alwaysOnTopAct = new QAction(makePinIcon(Qt::gray), "Always on top", this);
    alwaysOnTopAct->setCheckable(true);
    connect(alwaysOnTopAct, &QAction::toggled, this, &MainWindow::setAlwaysOnTop);
    infoMenu->addAction(alwaysOnTopAct);

    exitAct = new QAction(
        QIcon([]{
            QPixmap pix(16, 16);
            pix.fill(Qt::transparent);
            QPainter p(&pix);
            p.setRenderHint(QPainter::Antialiasing);
            QPen pen(QColor("#d32f2f"), 2);
            p.setPen(pen);
            p.setBrush(QColor("#d32f2f"));
            p.drawEllipse(4, 4, 8, 8);
            p.setPen(QPen(Qt::white, 2));
            p.drawLine(6, 6, 10, 10);
            p.drawLine(10, 6, 6, 10);
            return pix;
        }()),
        "Exit", this);
    exitAct->setShortcut(QKeySequence("Ctrl+W"));
    connect(exitAct, &QAction::triggered, this, &MainWindow::handleCtrlW);
    infoMenu->addAction(exitAct);

    ctrlWShortcut = new QShortcut(QKeySequence("Ctrl+W"), this);
    ctrlWShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(ctrlWShortcut, &QShortcut::activated, this, &MainWindow::handleCtrlW);

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    QAction *aboutAct = new QAction(getAppIcon(), "About", this);
    connect(aboutAct, &QAction::triggered, this, &MainWindow::showAbout);
    helpMenu->addAction(aboutAct);

    // --- Dark menu bar and menu styling ---
    QString menuStyle =
        "QMenuBar { background: #232629; color: #f0f0f0; }"
        "QMenuBar::item:selected { background: #3a3d41; }"
        "QMenu { background: #232629; color: #f0f0f0; }"
        "QMenu::item:selected { background: #3a3d41; }";
    menuBar()->setStyleSheet(menuStyle);
    infoMenu->setStyleSheet(menuStyle);
    helpMenu->setStyleSheet(menuStyle);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (quitting) {
        event->accept();
        return;
    }
    if (confirmQuit()) {
        quitting = true;
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::setAlwaysOnTop(bool on) {
    Qt::WindowFlags flags = windowFlags();
    if (on) {
        setWindowFlags(flags | Qt::WindowStaysOnTopHint);
        alwaysOnTopAct->setIcon(makePinIcon(Qt::red));
    } else {
        setWindowFlags(flags & ~Qt::WindowStaysOnTopHint);
        alwaysOnTopAct->setIcon(makePinIcon(Qt::gray));
    }
    show();
}

void MainWindow::tryQuit() {
    if (!quitting && confirmQuit()) {
        quitting = true;
        qApp->quit();
    }
}

void MainWindow::handleCtrlW() {
    tryQuit();
}

void MainWindow::showAbout() {
    QDialog dlg(this);
    dlg.setWindowTitle("About WinInfoApp");
    dlg.setWindowIcon(getAppIcon());
    dlg.setModal(true);
    dlg.resize(370, 240);

    QVBoxLayout *vbox = new QVBoxLayout(&dlg);

    QLabel *iconLabel = new QLabel;
    QPixmap iconPixmap = getAppIcon().pixmap(48, 48);
    iconLabel->setPixmap(iconPixmap);
    iconLabel->setAlignment(Qt::AlignCenter);
    vbox->addWidget(iconLabel);

    QTextBrowser *infoBrowser = new QTextBrowser;
    infoBrowser->setOpenExternalLinks(true);
    infoBrowser->setFrameShape(QFrame::StyledPanel);
    infoBrowser->setLineWidth(2);
    infoBrowser->setMidLineWidth(2);
    infoBrowser->setAlignment(Qt::AlignCenter);
    infoBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    infoBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    infoBrowser->setStyleSheet(
        "background: white; color: #232629; font-size: 15px;"
        "border: 2px solid black; border-radius: 12px;"
        "a { color: #0057b7; text-decoration: underline; }"
        "h3 { color: #0057b7; margin-bottom: 8px; }"
        "b { color: #232629; }"
    );
    infoBrowser->setHtml(QString(
        "<h3>WinInfoApp V. %1</h3>"
        "Copyleft 2025 Nalle Berg<br>"
        "<b>License:</b> <a href=\"https://www.gnu.org/licenses/old-licenses/gpl-2.0.html\">GPL V. 2.</a>"
        "<br><b>Source Code:</b> <a href=\"https://github.com/NalleBerg/WinInfoApp\">GitHub Repository</a>"
    ).arg(versionnumber));
    vbox->addWidget(infoBrowser);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addStretch();
    QPushButton *closeBtn = new QPushButton("Close");
    closeBtn->setStyleSheet(
        "background: #3a3d41; color: #f0f0f0; border-radius: 4px; padding: 4px 12px;"
        "font-weight: bold;"
        "font-size: 13px;"
        "min-width: 70px;"
    );
    hbox->addWidget(closeBtn);
    hbox->addStretch();
    vbox->addLayout(hbox);

    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    addCtrlWClose(&dlg);

    dlg.exec();
}

bool MainWindow::confirmQuit() {
    QDialog dlg(this);
    dlg.setWindowTitle("Confirm Exit");
    dlg.setWindowIcon(getAppIcon());
    dlg.setModal(true);
    dlg.resize(320, 160);

    QVBoxLayout *vbox = new QVBoxLayout(&dlg);

    QLabel *iconLabel = new QLabel;
    QPixmap iconPixmap = makeQuestionIcon().pixmap(32, 32);
    iconLabel->setPixmap(iconPixmap);
    iconLabel->setAlignment(Qt::AlignCenter);
    vbox->addWidget(iconLabel);

    QLabel *questionLabel = new QLabel(
        "<span style='color:#232629; font-size:14px;'>Are you sure you want to quit?</span>");
    questionLabel->setAlignment(Qt::AlignCenter);
    vbox->addWidget(questionLabel);

    QHBoxLayout *hbox = new QHBoxLayout;
    QPushButton *yesBtn = new QPushButton("Yes");
    QPushButton *noBtn = new QPushButton("No");
    yesBtn->setDefault(true);
    yesBtn->setStyleSheet(
        "background: #0057b7; color: #fff; border-radius: 4px; padding: 4px 16px; font-weight: bold;");
    noBtn->setStyleSheet(
        "background: #3a3d41; color: #f0f0f0; border-radius: 4px; padding: 4px 16px; font-weight: bold;");
    hbox->addStretch();
    hbox->addWidget(yesBtn);
    hbox->addWidget(noBtn);
    hbox->addStretch();
    vbox->addLayout(hbox);

    dlg.setStyleSheet("QDialog { border: 2px solid black; border-radius: 12px; }");

    bool result = false;
    connect(yesBtn, &QPushButton::clicked, [&]() { result = true; dlg.accept(); });
    connect(noBtn, &QPushButton::clicked, [&]() { result = false; dlg.reject(); });

    addCtrlWClose(&dlg);

    dlg.exec();
    return result;
}

// --- int main ---
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setWindowIcon(getAppIcon());
    MainWindow w;
    w.show();
    return app.exec();
}

#include "wininfoapp.moc"