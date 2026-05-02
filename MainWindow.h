#pragma once

#include <QButtonGroup>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

class QLineEdit;
class TestManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(TestManager *testManager, QWidget *parent = nullptr);

private slots:
    void rebuildTestList();
    void rebuildProgress();
    void rebuildCardArea();
    void updateHeader();

private:
    void buildUi();
    void applyTheme();
    void clearLayout(QLayout *layout);
    QWidget *createBlockWidget(const QVariantMap &block, QWidget *parent) const;
    QWidget *createOptionWidget(const QVariantMap &option, const QString &cardType, QWidget *parent);
    QWidget *createResultWidget(QWidget *parent);
    QPushButton *createActionButton(const QString &text, QWidget *parent) const;
    QColor colorForState(const QString &state) const;

    TestManager *m_testManager = nullptr;

    QListWidget *m_testsList = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_modeLabel = nullptr;
    QWidget *m_progressWidget = nullptr;
    QHBoxLayout *m_progressLayout = nullptr;
    QScrollArea *m_cardScrollArea = nullptr;
    QWidget *m_cardContent = nullptr;
    QVBoxLayout *m_cardLayout = nullptr;

    QPushButton *m_finishButton = nullptr;
    QPushButton *m_resetButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_backButton = nullptr;
    QPushButton *m_nextButton = nullptr;

    QButtonGroup *m_singleAnswerGroup = nullptr;
    QList<QMetaObject::Connection> m_rebuildConnections;
};
