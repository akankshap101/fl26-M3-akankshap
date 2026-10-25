#include "aiws/gui/main_window.hpp"
#include "aiws/document.hpp"
#include "aiws/gui/context_widget.hpp"
#include "aiws/gui/resettable.hpp"
#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/term_inspector_widget.hpp"
#include "aiws/gui/workspace_widget.hpp"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <exception>

namespace aiws::gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // TODO: Construct the M3 application window, compose the GUI components,
    // establish layouts/presentation, connect events, and establish initial state.
    setWindowTitle("AI Workspace");
    resize(900, 900);
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* layout = new QVBoxLayout(central);
    auto* heading = new QLabel("AI Workspace", central);
    heading->setStyleSheet("background-color: #243b53;" "color: white;""font-size: 22px;""font-weight: bold;""padding: 12px;");
    layout->addWidget(heading);
    workspace_widget_ = new WorkspaceWidget(central);
    layout->addWidget(workspace_widget_, 3);
    search_widget_ = new SearchWidget(central);
    layout->addWidget(search_widget_, 4);
    auto* bottom_layout = new QHBoxLayout();
    term_inspector_widget_ = new TermInspectorWidget(central);
    context_widget_ = new ContextWidget(central);
    bottom_layout->addWidget(term_inspector_widget_, 1);
    bottom_layout->addWidget(context_widget_, 1);
    layout->addLayout(bottom_layout, 3);
    auto* action_layout = new QHBoxLayout();
    status_label_ = new QLabel("Ready.", central);
    status_label_->setWordWrap(true);
    status_label_->setTextFormat(Qt::PlainText);
    reset_button_ = new QPushButton("Reset", central);
    exit_button_ = new QPushButton("Exit", central);
    reset_button_->setStyleSheet("background-color: #b8d8ed;""color: #102030;""padding: 6px 16px;");
    exit_button_->setStyleSheet("background-color: #8b2635;""color: white;""padding: 6px 16px;");
    action_layout->addWidget(status_label_, 1);
    action_layout->addWidget(reset_button_);
    action_layout->addWidget(exit_button_);
    layout->addLayout(action_layout);
    connect(workspace_widget_,&WorkspaceWidget::add_document_requested,this, &MainWindow::add_document);
    connect(workspace_widget_,&WorkspaceWidget::build_corpus_requested,this, &MainWindow::build_corpus);
    connect(workspace_widget_,&WorkspaceWidget::document_selected,this, &MainWindow::document_selected);
    connect(search_widget_, &SearchWidget::search_requested,this, &MainWindow::search_requested);
    connect(search_widget_, &SearchWidget::query_changed,this, &MainWindow::query_changed);
    connect(search_widget_,&SearchWidget::source_document_requested,this, &MainWindow::source_document_requested);
    connect(search_widget_,&SearchWidget::selected_chunk_changed,term_inspector_widget_,&TermInspectorWidget::set_selected_chunk);
    connect(term_inspector_widget_,&TermInspectorWidget::term_inspection_requested,this, &MainWindow::term_inspection_requested);
    connect(context_widget_,&ContextWidget::build_context_requested,this, &MainWindow::build_context_requested);
    connect(reset_button_, &QPushButton::clicked,this, &MainWindow::reset_application);
    connect(exit_button_, &QPushButton::clicked,this, &MainWindow::exit_application);

    update_action_states();

}

void MainWindow::set_exit_callback(ExitCallback callback) noexcept {
    // TODO
    exit_callback_ = callback;
}

void MainWindow::add_document() {
    // TODO
    const QString path = QFileDialog::getOpenFileName(this,"Add Document",QString(),"Text files (*.txt);;All files (*)");
    if(path.isEmpty()){
        return;
    }
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        QMessageBox::warning(this, "Open Error", file.errorString());return;
    }
    const QByteArray contents = file.readAll();
    if(file.error() != QFileDevice::NoError){
        QMessageBox::warning(this, "Read Error", file.errorString());
        return;
    }
    const QFileInfo info(path);
    const QString document_id = info.absoluteFilePath();
    const QString title = info.fileName();
    try{
        workspace_.add_document(Document(document_id.toStdString(),title.toStdString(),contents.toStdString()));
        workspace_widget_->add_document(title);
        invalidate_corpus();
        status_label_->setText("Document added. Build the corpus to process it.");
    }catch(const std::exception& error){
        QMessageBox::warning(this, "Add Document Error",QString::fromUtf8(error.what()));
    }
}


void MainWindow::build_corpus() {
    // TODO
    if(workspace_.documents().empty()){
        return;
    }
    invalidate_corpus();
    try {
        processing_core_.rebuild(workspace_);
        corpus_current_ = true;
        workspace_widget_->set_corpus_status(QString("Corpus: %1 chunks").arg(static_cast<qulonglong>(processing_core_.chunk_count())));
        update_action_states();
        status_label_->setText("Corpus built.");
    }catch(const std::exception& error){
        status_label_->setText("Corpus build failed.");
        QMessageBox::warning(this, "Build Error",QString::fromUtf8(error.what()));
    }
}

void MainWindow::document_selected(int row) {
    const auto& documents = workspace_.documents();
    if(row < 0 || static_cast<std::size_t>(row) >= documents.size()){
        workspace_widget_->clear_selected_document();
        return;
    }
    const Document& document = documents[static_cast<std::size_t>(row)];
    workspace_widget_->show_selected_document(
        QString::fromStdString(document.title()),
        QString::fromStdString(document.text()));
}

void MainWindow::search_requested(const QString& query, int k) {
    if(!corpus_current_ || query.trimmed().isEmpty()){
        return;
    }
    try{
        auto results = processing_core_.search(query.toStdString(), k);
        clear_processing_outputs();
        search_widget_->set_results(results);
        status_label_->setText(QString("Search complete: %1 results.").arg(static_cast<qulonglong>(results.size())));
    }catch(const std::exception& error){
        QMessageBox::warning(this, "Search Error", QString::fromUtf8(error.what()));
    }
}

void MainWindow::query_changed(const QString& query) {
    (void)query;
    // Only update the buttons when the query changes.
    update_action_states();
}

void MainWindow::build_context_requested(int token_budget) {
    if(!corpus_current_ || !search_widget_->has_query() || token_budget <= 0){
        return;
    }
    try{
        const auto items = processing_core_.build_context(search_widget_->query().toStdString(), search_widget_->result_count(),static_cast<std::size_t>(token_budget));
        context_widget_->set_context(items);
        status_label_->setText("Context built.");
    }catch(const std::exception& error){
        QMessageBox::warning(this, "Context Error", QString::fromUtf8(error.what()));
    }
}

void MainWindow::source_document_requested(const QString& document_id) {
    const auto& documents = workspace_.documents();
    for(std::size_t row = 0; row < documents.size(); row++){
        if(documents[row].id() == document_id.toStdString()){
            workspace_widget_->select_document(static_cast<int>(row));
            document_selected(static_cast<int>(row));
            return;
        }
    }
}

void MainWindow::term_inspection_requested(const QString& term, const QString& chunk_id) {
    if(!corpus_current_ || term.trimmed().isEmpty()){
        return;
    }
    try{
        const std::size_t df = processing_core_.document_frequency(term.toStdString());
        const bool has_chunk = !chunk_id.isEmpty();
        std::size_t tf = 0;
        if(has_chunk){
            tf = processing_core_.term_frequency(term.toStdString(), chunk_id.toStdString());
        }
        term_inspector_widget_->set_statistics(df, has_chunk, tf);
        status_label_->setText("Term inspected.");
    }catch(const std::exception& error){
        term_inspector_widget_->clear_statistics();
        QMessageBox::warning(this, "Inspection Error", QString::fromUtf8(error.what()));
    }
}

void MainWindow::reset_application() {
    corpus_current_ = false;
    workspace_.clear();
    processing_core_ = ProcessingCore();
    Resettable* components[] = {workspace_widget_,search_widget_,term_inspector_widget_,context_widget_};
    for(Resettable* component : components){
        component->reset();
    }
    update_action_states();
    status_label_->setText("Application reset.");
}

void MainWindow::exit_application() {
    if(exit_callback_ != nullptr){
        exit_callback_(this);
    }
}

void MainWindow::update_action_states() {
    workspace_widget_->set_build_corpus_enabled(!workspace_.documents().empty());
    const bool can_process_query = corpus_current_ && search_widget_->has_query();
    search_widget_->set_search_enabled(can_process_query);
    context_widget_->set_build_context_enabled(can_process_query);
    term_inspector_widget_->set_inspection_enabled(corpus_current_);
}

void MainWindow::invalidate_corpus() {
    corpus_current_ = false;
    workspace_widget_->set_corpus_status("Corpus: Not built");
    clear_processing_outputs();
    update_action_states();
}

void MainWindow::clear_processing_outputs() {
    search_widget_->clear_results();
    term_inspector_widget_->set_selected_chunk(QString());
    term_inspector_widget_->clear_statistics();
    context_widget_->clear_context();
}

}  // namespace aiws::gui
