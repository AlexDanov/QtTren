#include "MainWindow.h"

#include "TestManager.h"

#include <QCheckBox>
#include <QAbstractButton>
#include <QColor>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QRadioButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVariant>

MainWindow::MainWindow(TestManager *testManager, QWidget *parent)
    : QMainWindow(parent)
    , m_testManager(testManager)
{
    buildUi();
    applyTheme();

    connect(m_testManager, &TestManager::testsChanged, this, &MainWindow::rebuildTestList);
    connect(m_testManager, &TestManager::currentTestChanged, this, &MainWindow::updateHeader);
    connect(m_testManager, &TestManager::currentTestChanged, this, &MainWindow::rebuildCardArea);
    connect(m_testManager, &TestManager::currentCardChanged, this, &MainWindow::rebuildCardArea);
    connect(m_testManager, &TestManager::cardStatesChanged, this, &MainWindow::rebuildProgress);
    connect(m_testManager, &TestManager::finishedChanged, this, &MainWindow::rebuildCardArea);

    rebuildTestList();
    updateHeader();
    rebuildProgress();
    if (!m_testManager->tests().isEmpty() && !m_testManager->hasCurrentTest())
        m_testManager->selectTest(0);
    else
        rebuildCardArea();
}

void MainWindow::buildUi()
{
    setWindowTitle("QtTren - тестирующая система");

    auto *splitter = new QSplitter(this);
    splitter->setChildrenCollapsible(false);
    setCentralWidget(splitter);

    auto *leftPanel = new QWidget(splitter);
    leftPanel->setObjectName("leftPanel");
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(18, 18, 18, 18);
    leftLayout->setSpacing(14);

    auto *testsCaption = new QLabel("Набор тестов", leftPanel);
    testsCaption->setObjectName("sectionTitle");
    leftLayout->addWidget(testsCaption);

    m_testsList = new QListWidget(leftPanel);
    m_testsList->setObjectName("testsList");
    m_testsList->setSpacing(10);
    m_testsList->setFrameShape(QFrame::NoFrame);
    leftLayout->addWidget(m_testsList, 1);
    connect(m_testsList, &QListWidget::currentRowChanged, m_testManager, &TestManager::selectTest);

    auto *rightPanel = new QWidget(splitter);
    rightPanel->setObjectName("rightPanel");
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(22, 22, 22, 22);
    rightLayout->setSpacing(14);

    auto *headerLayout = new QHBoxLayout();
    auto *titleLayout = new QVBoxLayout();
    m_titleLabel = new QLabel(rightPanel);
    m_titleLabel->setObjectName("mainTitle");
    m_modeLabel = new QLabel(rightPanel);
    m_modeLabel->setObjectName("mutedLabel");
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_modeLabel);
    headerLayout->addLayout(titleLayout, 1);
    m_finishButton = createActionButton("Завершить", rightPanel);
    headerLayout->addWidget(m_finishButton);
    rightLayout->addLayout(headerLayout);
    connect(m_finishButton, &QPushButton::clicked, m_testManager, &TestManager::finishTest);

    m_progressWidget = new QWidget(rightPanel);
    m_progressLayout = new QHBoxLayout(m_progressWidget);
    m_progressLayout->setContentsMargins(0, 0, 0, 0);
    m_progressLayout->setSpacing(7);
    rightLayout->addWidget(m_progressWidget);

    m_cardScrollArea = new QScrollArea(rightPanel);
    m_cardScrollArea->setWidgetResizable(true);
    m_cardScrollArea->setObjectName("cardFrame");
    m_cardContent = new QWidget(m_cardScrollArea);
    m_cardLayout = new QVBoxLayout(m_cardContent);
    m_cardLayout->setContentsMargins(18, 18, 18, 18);
    m_cardLayout->setSpacing(14);
    m_cardScrollArea->setWidget(m_cardContent);
    rightLayout->addWidget(m_cardScrollArea, 1);

    auto *actionsLayout = new QHBoxLayout();
    actionsLayout->setSpacing(10);
    m_resetButton = createActionButton("Сбросить", rightPanel);
    m_saveButton = createActionButton("Сохранить", rightPanel);
    m_backButton = createActionButton("Назад", rightPanel);
    m_nextButton = createActionButton("Вперед", rightPanel);
    actionsLayout->addWidget(m_resetButton);
    actionsLayout->addWidget(m_saveButton);
    actionsLayout->addStretch(1);
    actionsLayout->addWidget(m_backButton);
    actionsLayout->addWidget(m_nextButton);
    rightLayout->addLayout(actionsLayout);

    connect(m_resetButton, &QPushButton::clicked, m_testManager, &TestManager::resetCurrentCard);
    connect(m_saveButton, &QPushButton::clicked, m_testManager, &TestManager::saveCurrentCard);
    connect(m_backButton, &QPushButton::clicked, m_testManager, &TestManager::previousCard);
    connect(m_nextButton, &QPushButton::clicked, m_testManager, &TestManager::nextCard);

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setSizes({330, 890});
}

void MainWindow::applyTheme()
{
    setStyleSheet(R"(
        QMainWindow, QWidget#rightPanel { background: #15191f; color: #f2f5f8; }
        QWidget#leftPanel { background: #20262e; border-right: 1px solid #36414e; }
        QLabel#sectionTitle { color: #f2f5f8; font-size: 24px; font-weight: 600; }
        QLabel#mainTitle { color: #f2f5f8; font-size: 26px; font-weight: 600; }
        QLabel#mutedLabel { color: #aeb8c4; font-size: 14px; }
        QLabel#cardTitle { color: #aeb8c4; font-size: 14px; }
        QListWidget#testsList { background: transparent; color: #f2f5f8; outline: 0; }
        QListWidget#testsList::item { background: #252c35; border: 1px solid #36414e; border-radius: 7px; padding: 8px 10px; margin: 2px; }
        QListWidget#testsList::item:selected { background: #26394a; border: 1px solid #6fb7ff; }
        QScrollArea#cardFrame { background: #20262e; border: 1px solid #36414e; border-radius: 8px; }
        QScrollArea#cardFrame > QWidget > QWidget { background: #20262e; }
        QPushButton { color: #111820; background: #e5edf5; border: 1px solid #ffffff; border-radius: 6px; padding: 8px 14px; font-weight: 600; min-height: 20px; }
        QPushButton:pressed { background: #b9d6f2; }
        QPushButton:disabled { color: #5f6872; background: #9ba4ad; border-color: #7b838c; }
        QRadioButton, QCheckBox { color: #f2f5f8; spacing: 8px; }
        QLineEdit { color: #f2f5f8; background: #242b34; border: 1px solid #36414e; border-radius: 7px; padding: 9px; selection-background-color: #6fb7ff; }
        QLineEdit:focus { border-color: #6fb7ff; }
        QScrollBar:vertical { background: #15191f; width: 12px; }
        QScrollBar::handle:vertical { background: #48525f; border-radius: 6px; min-height: 28px; }
    )");
}

void MainWindow::rebuildTestList()
{
    QSignalBlocker blocker(m_testsList);
    m_testsList->clear();

    const QVariantList tests = m_testManager->tests();
    for (const QVariant &testValue : tests) {
        const QVariantMap test = testValue.toMap();
        const QString mode = test.value("mode").toString() == "exercise" ? "учебный" : "проверка";
        const QString text = QString("%1\n%2\n%3/%4 отвечено  |  %5 верно  |  %6")
                                 .arg(test.value("title").toString(),
                                      test.value("description").toString())
                                 .arg(test.value("answered").toInt())
                                 .arg(test.value("count").toInt())
                                 .arg(test.value("correct").toInt())
                                 .arg(mode);
        auto *item = new QListWidgetItem(text, m_testsList);
        item->setSizeHint(QSize(290, 92));
        if (test.value("active").toBool())
            m_testsList->setCurrentItem(item);
    }
}

void MainWindow::rebuildProgress()
{
    clearLayout(m_progressLayout);

    const QVariantList states = m_testManager->cardStates();
    for (int i = 0; i < states.size(); ++i) {
        const QVariantMap state = states.at(i).toMap();
        auto *button = new QPushButton(QString::number(state.value("number").toInt()), m_progressWidget);
        button->setFixedSize(34, 34);
        const QColor color = colorForState(state.value("state").toString());
        const int borderWidth = state.value("active").toBool() ? 3 : 1;
        button->setStyleSheet(QString("QPushButton { color: #101820; background: %1; border: %2px solid %3; "
                                      "border-radius: 3px; padding: 0; font-weight: 700; }")
                                  .arg(color.name())
                                  .arg(borderWidth)
                                  .arg(state.value("active").toBool() ? "#ffffff" : "#222931"));
        connect(button, &QPushButton::clicked, this, [this, i]() { m_testManager->selectCard(i); });
        m_progressLayout->addWidget(button);
    }
    m_progressLayout->addStretch(1);
}

void MainWindow::rebuildCardArea()
{
    clearLayout(m_cardLayout);
    updateHeader();

    if (!m_testManager->hasCurrentTest()) {
        auto *empty = new QLabel("JSON-тесты не найдены. Добавьте файлы в папку tests.", m_cardContent);
        empty->setObjectName("mutedLabel");
        m_cardLayout->addWidget(empty);
        return;
    }

    if (m_testManager->finished()) {
        m_cardLayout->addWidget(createResultWidget(m_cardContent));
        m_cardLayout->addStretch(1);
        return;
    }

    const QVariantMap card = m_testManager->currentCard();
    auto *caption = new QLabel(QString("Карточка %1 из %2")
                                   .arg(m_testManager->currentCardIndex() + 1)
                                   .arg(m_testManager->cardCount()),
                               m_cardContent);
    caption->setObjectName("cardTitle");
    m_cardLayout->addWidget(caption);

    for (const QVariant &block : card.value("description").toList())
        m_cardLayout->addWidget(createBlockWidget(block.toMap(), m_cardContent));

    auto *line = new QFrame(m_cardContent);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #36414e;");
    m_cardLayout->addWidget(line);

    for (const QVariant &block : card.value("question").toList())
        m_cardLayout->addWidget(createBlockWidget(block.toMap(), m_cardContent));

    const QString cardType = card.value("type").toString();
    delete m_singleAnswerGroup;
    m_singleAnswerGroup = nullptr;
    if (cardType == "single")
        m_singleAnswerGroup = new QButtonGroup(this);

    if (cardType == "text") {
        auto *answerEdit = new QLineEdit(m_cardContent);
        answerEdit->setPlaceholderText("Введите число или слово");
        answerEdit->setText(m_testManager->textAnswer());
        connect(answerEdit, &QLineEdit::editingFinished, this, [this, answerEdit]() {
            m_testManager->updateTextAnswer(answerEdit->text());
        });
        m_cardLayout->addWidget(answerEdit);
    } else {
        for (const QVariant &option : card.value("options").toList())
            m_cardLayout->addWidget(createOptionWidget(option.toMap(), cardType, m_cardContent));
    }

    m_cardLayout->addStretch(1);
}

void MainWindow::updateHeader()
{
    m_titleLabel->setText(m_testManager->currentTestTitle().isEmpty() ? "Тесты не найдены" : m_testManager->currentTestTitle());
    m_modeLabel->setText(m_testManager->currentTestMode() == "exercise"
                             ? "Учебный режим: правильные ответы подсвечиваются сразу"
                             : "Проверка знаний: правильность видна в итоговой статистике");
    const bool hasTest = m_testManager->hasCurrentTest();
    const bool finished = m_testManager->finished();
    m_finishButton->setEnabled(hasTest && !finished);
    m_resetButton->setEnabled(hasTest && !finished);
    m_saveButton->setEnabled(hasTest && !finished);
    m_backButton->setEnabled(hasTest && !finished && m_testManager->currentCardIndex() > 0);
    m_nextButton->setEnabled(hasTest && !finished);
    m_nextButton->setText(hasTest && m_testManager->currentCardIndex() + 1 >= m_testManager->cardCount() ? "Итоги" : "Вперед");
}

void MainWindow::clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        if (QLayout *childLayout = item->layout())
            clearLayout(childLayout);
        delete item;
    }
}

QWidget *MainWindow::createBlockWidget(const QVariantMap &block, QWidget *parent) const
{
    auto *label = new QLabel(parent);
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    label->setOpenExternalLinks(false);
    label->setText(block.value("html").toString());
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

QWidget *MainWindow::createOptionWidget(const QVariantMap &option, const QString &cardType, QWidget *parent)
{
    auto *frame = new QFrame(parent);
    frame->setObjectName("optionFrame");
    frame->setStyleSheet(QString("QFrame#optionFrame { background: %1; border: 1px solid %2; border-radius: 7px; }")
                             .arg(m_testManager->optionSelected(option.value("id").toString()) ? "#2f4150" : "#242b34")
                             .arg(m_testManager->optionSelected(option.value("id").toString()) ? "#6fb7ff" : "#36414e"));

    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(9, 9, 9, 9);
    layout->setSpacing(10);

    const QString optionId = option.value("id").toString();
    QAbstractButton *selector = nullptr;
    if (cardType == "multi") {
        auto *checkBox = new QCheckBox(frame);
        checkBox->setChecked(m_testManager->optionSelected(optionId));
        connect(checkBox, &QCheckBox::toggled, this, [this, optionId](bool checked) {
            m_testManager->toggleMultiAnswer(optionId, checked);
        });
        selector = checkBox;
    } else {
        auto *radioButton = new QRadioButton(frame);
        radioButton->setChecked(m_testManager->optionSelected(optionId));
        m_singleAnswerGroup->addButton(radioButton);
        connect(radioButton, &QRadioButton::clicked, this, [this, optionId]() {
            m_testManager->updateSingleAnswer(optionId);
        });
        selector = radioButton;
    }
    selector->setFixedWidth(28);
    layout->addWidget(selector);

    auto *blocksLayout = new QVBoxLayout();
    blocksLayout->setSpacing(2);
    for (const QVariant &block : option.value("blocks").toList())
        blocksLayout->addWidget(createBlockWidget(block.toMap(), frame));
    layout->addLayout(blocksLayout, 1);

    return frame;
}

QWidget *MainWindow::createResultWidget(QWidget *parent)
{
    const QVariantMap stats = m_testManager->resultStats();
    auto *widget = new QWidget(parent);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto *title = new QLabel("Статистика прохождения", widget);
    title->setObjectName("mainTitle");
    layout->addWidget(title);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(26);
    grid->setVerticalSpacing(14);
    const QString elapsed = QString("%1 мин %2 сек")
                                .arg(stats.value("elapsedSeconds").toInt() / 60)
                                .arg(stats.value("elapsedSeconds").toInt() % 60);
    const QStringList labels = {"Правильных ответов", "Дано ответов", "Время прохождения"};
    const QStringList values = {
        QString("%1 из %2").arg(stats.value("correct").toInt()).arg(stats.value("total").toInt()),
        QString("%1 из %2").arg(stats.value("answered").toInt()).arg(stats.value("total").toInt()),
        elapsed};
    for (int row = 0; row < labels.size(); ++row) {
        auto *label = new QLabel(labels.at(row), widget);
        label->setObjectName("mutedLabel");
        auto *value = new QLabel(values.at(row), widget);
        value->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: 600;")
                                 .arg(row == 0 ? "#65d184" : "#f2f5f8"));
        grid->addWidget(label, row, 0);
        grid->addWidget(value, row, 1);
    }
    layout->addLayout(grid);
    layout->addStretch(1);

    auto *backToCards = createActionButton("Вернуться к карточкам", widget);
    connect(backToCards, &QPushButton::clicked, this, [this]() {
        m_testManager->selectCard(qMax(0, m_testManager->currentCardIndex()));
    });
    layout->addWidget(backToCards, 0, Qt::AlignRight);
    return widget;
}

QPushButton *MainWindow::createActionButton(const QString &text, QWidget *parent) const
{
    auto *button = new QPushButton(text, parent);
    button->setMinimumHeight(38);
    return button;
}

QColor MainWindow::colorForState(const QString &state) const
{
    if (state == "correct")
        return QColor("#65d184");
    if (state == "selected")
        return QColor("#f0c76c");
    return QColor("#48525f");
}
