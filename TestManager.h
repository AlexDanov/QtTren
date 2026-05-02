#pragma once

#include <QObject>
#include <QDateTime>
#include <QVariantList>

class TestManager : public QObject
{
    Q_OBJECT

public:
    explicit TestManager(QObject *parent = nullptr);

    QVariantList tests() const;
    QVariantMap currentCard() const;
    QVariantList cardStates() const;
    QString currentTestTitle() const;
    QString currentTestMode() const;
    int currentCardIndex() const;
    int cardCount() const;
    bool hasCurrentTest() const;
    bool finished() const;
    QVariantMap resultStats() const;

    void loadTests();
    void selectTest(int index);
    void selectCard(int index);
    void previousCard();
    void nextCard();
    void resetCurrentCard();
    void saveCurrentCard();
    void updateSingleAnswer(const QString &optionId);
    void toggleMultiAnswer(const QString &optionId, bool checked);
    void updateTextAnswer(const QString &text);
    void finishTest();
    bool optionSelected(const QString &optionId) const;
    QString textAnswer() const;

signals:
    void testsChanged();
    void currentTestChanged();
    void currentCardChanged();
    void cardStatesChanged();
    void finishedChanged();

private:
    struct ContentBlock {
        QString type;
        QString language;
        QString text;
    };

    struct AnswerOption {
        QString id;
        QList<ContentBlock> blocks;
    };

    struct Card {
        QString id;
        QString type;
        QList<ContentBlock> description;
        QList<ContentBlock> question;
        QList<AnswerOption> options;
        QVariant correct;
    };

    struct Test {
        QString id;
        QString title;
        QString description;
        QString mode;
        QList<Card> cards;
    };

    void loadFromDirectory(const QString &directoryPath);
    bool parseTestFile(const QString &filePath, Test *test) const;
    QVariantMap cardToVariant(const Card &card) const;
    QVariantList blocksToVariant(const QList<ContentBlock> &blocks) const;
    QString renderBlockHtml(const ContentBlock &block) const;
    QString highlightCode(const QString &code, const QString &language) const;
    QString stateForCard(int index) const;
    bool isCurrentCardCorrect() const;
    bool isCardCorrect(int index) const;
    void markAnswerChanged();
    void logCurrentCard(const QString &event);
    void appendLogRecord(const QVariantMap &record) const;
    QVariant normalizedAnswer(int cardIndex) const;
    void emitStateChanged();

    QList<Test> m_tests;
    int m_currentTest = -1;
    int m_currentCard = -1;
    bool m_finished = false;
    QDateTime m_startedAt;
    QVector<QVariant> m_answers;
    QVector<bool> m_answered;
    QVector<bool> m_dirty;
};
