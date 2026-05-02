#include "TestManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

TestManager::TestManager(QObject *parent)
    : QObject(parent)
{
}

QVariantList TestManager::tests() const
{
    QVariantList result;
    for (int i = 0; i < m_tests.size(); ++i) {
        const Test &test = m_tests.at(i);
        int answered = 0;
        int correct = 0;
        if (i == m_currentTest) {
            for (int card = 0; card < m_answered.size(); ++card) {
                if (m_answered.at(card))
                    ++answered;
                if (isCardCorrect(card))
                    ++correct;
            }
        }

        QVariantMap item;
        item["id"] = test.id;
        item["title"] = test.title;
        item["description"] = test.description;
        item["mode"] = test.mode;
        item["count"] = test.cards.size();
        item["answered"] = answered;
        item["correct"] = correct;
        item["active"] = i == m_currentTest;
        result << item;
    }
    return result;
}

QVariantMap TestManager::currentCard() const
{
    if (!hasCurrentTest() || m_currentCard < 0 || m_currentCard >= cardCount())
        return {};
    return cardToVariant(m_tests.at(m_currentTest).cards.at(m_currentCard));
}

QVariantList TestManager::cardStates() const
{
    QVariantList states;
    for (int i = 0; i < cardCount(); ++i) {
        QVariantMap item;
        item["number"] = i + 1;
        item["state"] = stateForCard(i);
        item["active"] = i == m_currentCard && !m_finished;
        states << item;
    }
    return states;
}

QString TestManager::currentTestTitle() const
{
    return hasCurrentTest() ? m_tests.at(m_currentTest).title : QString();
}

QString TestManager::currentTestMode() const
{
    return hasCurrentTest() ? m_tests.at(m_currentTest).mode : QString();
}

int TestManager::currentCardIndex() const
{
    return m_currentCard;
}

int TestManager::cardCount() const
{
    return hasCurrentTest() ? m_tests.at(m_currentTest).cards.size() : 0;
}

bool TestManager::hasCurrentTest() const
{
    return m_currentTest >= 0 && m_currentTest < m_tests.size();
}

bool TestManager::finished() const
{
    return m_finished;
}

QVariantMap TestManager::resultStats() const
{
    QVariantMap stats;
    stats["total"] = cardCount();

    int answered = 0;
    int correct = 0;
    for (int i = 0; i < m_answered.size(); ++i) {
        if (m_answered.at(i))
            ++answered;
        if (isCardCorrect(i))
            ++correct;
    }

    stats["answered"] = answered;
    stats["correct"] = correct;
    stats["elapsedSeconds"] = m_startedAt.isValid() ? m_startedAt.secsTo(QDateTime::currentDateTimeUtc()) : 0;
    return stats;
}

void TestManager::loadTests()
{
    m_tests.clear();
    const QString appTests = QCoreApplication::applicationDirPath() + "/tests";
    const QString sourceTests = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../../tests");

    loadFromDirectory(appTests);
    if (m_tests.isEmpty())
        loadFromDirectory(sourceTests);
    if (m_tests.isEmpty())
        loadFromDirectory(QDir::current().absoluteFilePath("tests"));

    emit testsChanged();
}

void TestManager::selectTest(int index)
{
    if (index < 0 || index >= m_tests.size())
        return;

    if (hasCurrentTest())
        logCurrentCard("leave");

    m_currentTest = index;
    m_currentCard = 0;
    m_finished = false;
    m_startedAt = QDateTime::currentDateTimeUtc();
    const int count = cardCount();
    m_answers = QVector<QVariant>(count);
    m_answered = QVector<bool>(count, false);
    m_dirty = QVector<bool>(count, false);

    emit currentTestChanged();
    emit currentCardChanged();
    emit cardStatesChanged();
    emit finishedChanged();
    emit testsChanged();
}

void TestManager::selectCard(int index)
{
    if (!hasCurrentTest() || index < 0 || index >= cardCount())
        return;

    logCurrentCard("leave");
    m_currentCard = index;
    m_finished = false;
    emit currentCardChanged();
    emit cardStatesChanged();
    emit finishedChanged();
}

void TestManager::previousCard()
{
    if (m_currentCard > 0)
        selectCard(m_currentCard - 1);
}

void TestManager::nextCard()
{
    if (!hasCurrentTest())
        return;
    if (m_currentCard + 1 < cardCount()) {
        selectCard(m_currentCard + 1);
    } else {
        finishTest();
    }
}

void TestManager::resetCurrentCard()
{
    if (!hasCurrentTest() || m_currentCard < 0)
        return;
    m_answers[m_currentCard] = QVariant();
    m_answered[m_currentCard] = false;
    m_dirty[m_currentCard] = false;
    logCurrentCard("reset");
    emitStateChanged();
}

void TestManager::saveCurrentCard()
{
    logCurrentCard("save");
    emitStateChanged();
}

void TestManager::updateSingleAnswer(const QString &optionId)
{
    if (!hasCurrentTest() || m_currentCard < 0)
        return;
    m_answers[m_currentCard] = optionId;
    markAnswerChanged();
}

void TestManager::toggleMultiAnswer(const QString &optionId, bool checked)
{
    if (!hasCurrentTest() || m_currentCard < 0)
        return;

    QStringList selected = m_answers[m_currentCard].toStringList();
    if (checked && !selected.contains(optionId)) {
        selected << optionId;
    } else if (!checked) {
        selected.removeAll(optionId);
    }
    selected.sort();
    m_answers[m_currentCard] = selected;
    m_answered[m_currentCard] = !selected.isEmpty();
    m_dirty[m_currentCard] = true;
    emitStateChanged();
}

void TestManager::updateTextAnswer(const QString &text)
{
    if (!hasCurrentTest() || m_currentCard < 0)
        return;
    m_answers[m_currentCard] = text;
    m_answered[m_currentCard] = !text.trimmed().isEmpty();
    m_dirty[m_currentCard] = true;
    emitStateChanged();
}

void TestManager::finishTest()
{
    if (!hasCurrentTest())
        return;

    logCurrentCard("leave");
    m_finished = true;
    QVariantMap record;
    record["event"] = "finish";
    record["testId"] = m_tests.at(m_currentTest).id;
    record["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    record["stats"] = resultStats();
    appendLogRecord(record);

    emit finishedChanged();
    emit cardStatesChanged();
    emit testsChanged();
}

bool TestManager::optionSelected(const QString &optionId) const
{
    if (!hasCurrentTest() || m_currentCard < 0)
        return false;
    const QVariant answer = m_answers.at(m_currentCard);
    if (answer.typeId() == QMetaType::QStringList)
        return answer.toStringList().contains(optionId);
    return answer.toString() == optionId;
}

QString TestManager::textAnswer() const
{
    if (!hasCurrentTest() || m_currentCard < 0)
        return QString();
    return m_answers.at(m_currentCard).toString();
}

void TestManager::loadFromDirectory(const QString &directoryPath)
{
    QDir dir(directoryPath);
    if (!dir.exists())
        return;

    const QFileInfoList files = dir.entryInfoList(QStringList() << "*.json", QDir::Files, QDir::Name);
    for (const QFileInfo &file : files) {
        Test test;
        if (parseTestFile(file.absoluteFilePath(), &test))
            m_tests << test;
    }
}

bool TestManager::parseTestFile(const QString &filePath, Test *test) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return false;

    const QJsonObject root = document.object();
    test->id = root.value("id").toString();
    test->title = root.value("title").toString();
    test->description = root.value("description").toString();
    test->mode = root.value("mode").toString("exercise");

    const auto parseBlocks = [](const QJsonArray &array) {
        QList<ContentBlock> blocks;
        for (const QJsonValue &value : array) {
            const QJsonObject object = value.toObject();
            ContentBlock block;
            block.type = object.value("type").toString("text");
            block.language = object.value("language").toString();
            block.text = object.value("text").toString();
            blocks << block;
        }
        return blocks;
    };

    const QJsonArray cards = root.value("cards").toArray();
    for (const QJsonValue &cardValue : cards) {
        const QJsonObject cardObject = cardValue.toObject();
        Card card;
        card.id = cardObject.value("id").toString();
        card.type = cardObject.value("type").toString("single");
        card.description = parseBlocks(cardObject.value("description").toArray());
        card.question = parseBlocks(cardObject.value("question").toArray());

        const QJsonArray options = cardObject.value("options").toArray();
        for (const QJsonValue &optionValue : options) {
            const QJsonObject optionObject = optionValue.toObject();
            AnswerOption option;
            option.id = optionObject.value("id").toString();
            option.blocks = parseBlocks(optionObject.value("blocks").toArray());
            card.options << option;
        }

        const QJsonValue correct = cardObject.value("correct");
        if (correct.isArray()) {
            QStringList list;
            for (const QJsonValue &item : correct.toArray())
                list << item.toString();
            list.sort();
            card.correct = list;
        } else {
            card.correct = correct.toVariant();
        }
        test->cards << card;
    }

    return !test->id.isEmpty() && !test->cards.isEmpty();
}

QVariantMap TestManager::cardToVariant(const Card &card) const
{
    QVariantMap result;
    result["id"] = card.id;
    result["type"] = card.type;
    result["description"] = blocksToVariant(card.description);
    result["question"] = blocksToVariant(card.question);

    QVariantList options;
    for (const AnswerOption &option : card.options) {
        QVariantMap item;
        item["id"] = option.id;
        item["blocks"] = blocksToVariant(option.blocks);
        options << item;
    }
    result["options"] = options;
    return result;
}

QVariantList TestManager::blocksToVariant(const QList<ContentBlock> &blocks) const
{
    QVariantList result;
    for (const ContentBlock &block : blocks) {
        QVariantMap item;
        item["type"] = block.type;
        item["language"] = block.language;
        item["text"] = block.text;
        item["html"] = renderBlockHtml(block);
        result << item;
    }
    return result;
}

QString TestManager::renderBlockHtml(const ContentBlock &block) const
{
    if (block.type == "code") {
        return QString("<pre style=\"margin:8px 0; padding:12px; background:#101820; color:#d6deeb; "
                       "border:1px solid #2a3642; border-radius:6px; font-family:'Cascadia Mono','Consolas',monospace; "
                       "font-size:14px; white-space:pre-wrap;\">%1</pre>")
            .arg(highlightCode(block.text, block.language));
    }

    return QString("<p style=\"margin:6px 0; color:#f1f4f8; font-size:16px; line-height:1.35;\">%1</p>")
        .arg(block.text.toHtmlEscaped().replace("\n", "<br>"));
}

QString TestManager::highlightCode(const QString &code, const QString &language) const
{
    QString highlighted = code.toHtmlEscaped();
    QStringList keywords;
    if (language.compare("python", Qt::CaseInsensitive) == 0) {
        keywords = {"and", "as", "assert", "break", "class", "continue", "def", "elif", "else", "False", "for",
                    "from", "if", "import", "in", "is", "lambda", "None", "not", "or", "pass", "return", "True",
                    "try", "while", "with", "yield"};
    } else {
        keywords = {"auto", "bool", "break", "case", "char", "class", "const", "continue", "default", "delete",
                    "double", "else", "enum", "false", "float", "for", "if", "include", "int", "long", "namespace",
                    "new", "nullptr", "private", "public", "return", "short", "sizeof", "static", "std", "struct",
                    "switch", "true", "using", "void", "while"};
    }

    for (const QString &keyword : keywords) {
        highlighted.replace(QRegularExpression(QString("\\b%1\\b").arg(QRegularExpression::escape(keyword))),
                            QString("<span style=\"color:#569cd6; font-weight:600;\">%1</span>").arg(keyword));
    }
    return highlighted;
}

QString TestManager::stateForCard(int index) const
{
    if (index < 0 || index >= m_answered.size() || !m_answered.at(index))
        return "empty";
    if (hasCurrentTest() && m_tests.at(m_currentTest).mode == "exercise" && isCardCorrect(index))
        return "correct";
    return "selected";
}

bool TestManager::isCurrentCardCorrect() const
{
    return isCardCorrect(m_currentCard);
}

bool TestManager::isCardCorrect(int index) const
{
    if (!hasCurrentTest() || index < 0 || index >= cardCount() || !m_answered.value(index))
        return false;

    const Card &card = m_tests.at(m_currentTest).cards.at(index);
    const QVariant answer = normalizedAnswer(index);
    if (card.type == "text") {
        return answer.toString().trimmed().compare(card.correct.toString().trimmed(), Qt::CaseInsensitive) == 0;
    }
    if (card.type == "multi") {
        return answer.toStringList() == card.correct.toStringList();
    }
    return answer.toString() == card.correct.toString();
}

void TestManager::markAnswerChanged()
{
    m_answered[m_currentCard] = true;
    m_dirty[m_currentCard] = true;
    emitStateChanged();
}

void TestManager::logCurrentCard(const QString &event)
{
    if (!hasCurrentTest() || m_currentCard < 0)
        return;
    if ((event == "leave" || event == "save") && !m_dirty.value(m_currentCard) && event != "save")
        return;

    QVariantMap record;
    record["event"] = event;
    record["testId"] = m_tests.at(m_currentTest).id;
    record["cardId"] = m_tests.at(m_currentTest).cards.at(m_currentCard).id;
    record["answer"] = normalizedAnswer(m_currentCard);
    record["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    appendLogRecord(record);
    if (m_currentCard >= 0 && m_currentCard < m_dirty.size())
        m_dirty[m_currentCard] = false;
}

void TestManager::appendLogRecord(const QVariantMap &record) const
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (basePath.isEmpty())
        basePath = QCoreApplication::applicationDirPath();
    QDir().mkpath(basePath + "/logs");
    const QString logPath = basePath + "/logs/attempts.json";

    QJsonArray records;
    QFile readFile(logPath);
    if (readFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(readFile.readAll());
        if (document.isArray())
            records = document.array();
    }

    records.append(QJsonObject::fromVariantMap(record));
    QFile writeFile(logPath);
    if (writeFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        writeFile.write(QJsonDocument(records).toJson(QJsonDocument::Indented));
}

QVariant TestManager::normalizedAnswer(int cardIndex) const
{
    if (cardIndex < 0 || cardIndex >= m_answers.size())
        return QVariant();

    const Card &card = m_tests.at(m_currentTest).cards.at(cardIndex);
    if (card.type == "multi") {
        QStringList list = m_answers.at(cardIndex).toStringList();
        list.sort();
        return list;
    }
    if (card.type == "text")
        return m_answers.at(cardIndex).toString().trimmed();
    return m_answers.at(cardIndex);
}

void TestManager::emitStateChanged()
{
    emit currentCardChanged();
    emit cardStatesChanged();
    emit testsChanged();
}
