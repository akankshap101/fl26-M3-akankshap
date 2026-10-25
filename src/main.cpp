#include "aiws/gui/main_window.hpp"

#include <QApplication>
#include <QMessageBox>

// TODO: Define the ExitCallback function used to confirm an Exit request.
void confirm_exit(QWidget* parent){
    QMessageBox::StandardButton answer = QMessageBox::question(parent,"Exit","Are you sure?",QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if(answer == QMessageBox::Yes){
        QApplication::quit();
    }
}
int main(int argc, char* argv[]) {
    // TODO: Create the QApplication, construct/configure the MainWindow,
    // register the ExitCallback, show the window, and run the event loop.
    QApplication app(argc, argv);

    aiws::gui::MainWindow window;
    window.setWindowTitle("AI workspace");
    window.resize(900,900);
    window.set_exit_callback(&confirm_exit);
    window.show();

    return app.exec();
}
