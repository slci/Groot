#include <QtTest>
#include <QDomDocument>
#include <QFile>
#include <QApplication>
#include <QSlider>
#include <QSpinBox>
#include <QTableView>
#include <QTemporaryDir>
#include <QTreeWidget>

#include <behaviortree_cpp/bt_factory.h>
#include <behaviortree_cpp/loggers/bt_file_logger_v2.h>
#include <behaviortree_cpp/loggers/groot2_publisher.h>
#include <behaviortree_cpp/xml_parsing.h>

#include "bt_editor/XML_utilities.hpp"
#include "bt_editor/mainwindow.h"
#include "bt_editor/sidepanel_replay.h"
#include "bt_editor/sidepanel_monitor.h"
#include "bt_editor/utils.h"

namespace {

const char* kModelLessXML = R"(
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence name="main">
      <CheckBattery level="{battery}"/>
      <SayHello message="Hi"/>
    </Sequence>
  </BehaviorTree>
</root>)";

const char* kSubTreeXML = R"(
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <Action ID="DoThing" message="Hi"/>
      <SubTree ID="Worker" message="{message}"/>
    </Sequence>
  </BehaviorTree>
  <BehaviorTree ID="Worker">
    <Action ID="DoThing" message="From subtree"/>
  </BehaviorTree>
  <TreeNodesModel>
    <Action ID="DoThing"><input_port name="message" type="std::string"/></Action>
  </TreeNodesModel>
</root>)";

QDomDocument parseXML(const QString& text)
{
    QDomDocument document;
    if (!document.setContent(text))
        throw std::runtime_error("Invalid test XML");
    return document;
}

}  // namespace

class V4CompatTest : public QObject
{
    Q_OBJECT

private slots:
    void modelLessRoundTrip();
    void subtreeRoundTrip();
    void runtimeTreeAndLogReplay();
    void runtimeMonitor();
    void desktopWorkflow();
};

void V4CompatTest::modelLessRoundTrip()
{
    auto doc = parseXML(kModelLessXML);
    auto models = ReadTreeNodesModel(doc.documentElement());
    QVERIFY(models.count("CheckBattery") == 1);
    QVERIFY(models.at("CheckBattery").inferred_from_usage);
    QVERIFY(models.count("SayHello") == 1);
    std::vector<QString> errors;
    QVERIFY2(VerifyXML(doc, BuiltinNodeModels(), errors),
             errors.empty() ? "XML validation failed" : errors.front().toUtf8().constData());

    MainWindow window(GraphicMode::EDITOR);
    window.loadFromXML(kModelLessXML);
    QVERIFY(window.getTabByName("MainTree"));
    auto saved = window.saveToXML();
    QVERIFY(saved.contains("BTCPP_format=\"4\""));
    QVERIFY(saved.contains("<CheckBattery"));
    QVERIFY(saved.contains("<SayHello"));
    QVERIFY(!saved.contains("<Action ID=\"CheckBattery\""));
    auto saved_doc = parseXML(saved);
    models = ReadTreeNodesModel(saved_doc.documentElement());
    QVERIFY(models.at("CheckBattery").inferred_from_usage);

    BT::BehaviorTreeFactory factory;
    factory.registerSimpleCondition("CheckBattery", [](BT::TreeNode&) {
        return BT::NodeStatus::SUCCESS;
    }, {BT::InputPort<int>("level")});
    factory.registerSimpleAction("SayHello", [](BT::TreeNode&) {
        return BT::NodeStatus::SUCCESS;
    }, {BT::InputPort<std::string>("message")});
    auto blackboard = BT::Blackboard::create();
    blackboard->set("battery", 80);
    auto tree = factory.createTreeFromText(saved.toStdString(), blackboard);
    QCOMPARE(tree.tickOnce(), BT::NodeStatus::SUCCESS);
}

void V4CompatTest::subtreeRoundTrip()
{
    auto doc = parseXML(kSubTreeXML);
    std::vector<QString> errors;
    QVERIFY2(VerifyXML(doc, BuiltinNodeModels(), errors),
             errors.empty() ? "XML validation failed" : errors.front().toUtf8().constData());
    MainWindow window(GraphicMode::EDITOR);
    window.loadFromXML(kSubTreeXML);
    QVERIFY(window.getTabByName("MainTree"));
    QVERIFY(window.getTabByName("Worker"));
    const auto saved = window.saveToXML();
    QVERIFY(saved.contains("<SubTree ID=\"Worker\""));
    QVERIFY(saved.contains("<Action ID=\"DoThing\""));
    auto saved_doc = parseXML(saved);
    errors.clear();
    QVERIFY2(VerifyXML(saved_doc, BuiltinNodeModels(), errors),
             errors.empty() ? "Saved XML validation failed" : errors.front().toUtf8().constData());
    BT::BehaviorTreeFactory factory;
    factory.registerSimpleAction("DoThing", [](BT::TreeNode&) {
        return BT::NodeStatus::SUCCESS;
    }, {BT::InputPort<std::string>("message")});
    auto tree = factory.createTreeFromText(saved.toStdString());
    QCOMPARE(tree.tickOnce(), BT::NodeStatus::SUCCESS);

    auto visual_tree = BuildTreeFromScene(window.getTabByName("MainTree")->scene());
    const auto* subtree_node = visual_tree.findFirstNode("Worker");
    QVERIFY(subtree_node);
    auto* subtree_model = dynamic_cast<SubtreeNodeModel*>(
        subtree_node->graphic_node->nodeDataModel());
    QVERIFY(subtree_model);
    subtree_model->expandButton()->click();
    QVERIFY(subtree_model->expanded());
    const auto expanded_xml = window.saveToXML();
    auto expanded_tree = factory.createTreeFromText(expanded_xml.toStdString());
    QCOMPARE(expanded_tree.tickOnce(), BT::NodeStatus::SUCCESS);
}

void V4CompatTest::runtimeTreeAndLogReplay()
{
    BT::BehaviorTreeFactory factory;
    factory.registerSimpleAction("DoThing", [](BT::TreeNode&) {
        return BT::NodeStatus::SUCCESS;
    }, {BT::InputPort<std::string>("message")});
    auto tree = factory.createTreeFromText(kSubTreeXML);
    const auto runtime_xml = BT::WriteTreeToXML(tree, true, true);
    const auto doc = parseXML(QString::fromStdString(runtime_xml));
    auto result = BuildTreeFromRuntimeXML(doc);
    QCOMPARE(result.first.nodesCount(), size_t(5));
    QCOMPARE(result.second.size(), size_t(4));
    QCOMPARE(result.first.node(0)->model.registration_ID, QString("Root"));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("tree.btlog");
    {
        BT::FileLogger2 logger(tree, path.toStdString());
        QCOMPARE(tree.tickOnce(), BT::NodeStatus::SUCCESS);
        logger.flush();
    }
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto contents = file.readAll();
    QVERIFY(contents.startsWith("BTCPP4-FileLogger2"));

    MainWindow window(GraphicMode::REPLAY);
    auto* replay = window.findChild<SidepanelReplay*>();
    QVERIFY(replay);
    replay->loadLog(contents);
    QVERIFY(replay->transitionsCount() > 0);
    QVERIFY(window.getTabByName("BehaviorTree"));
}

void V4CompatTest::runtimeMonitor()
{
    BT::BehaviorTreeFactory factory;
    factory.registerSimpleAction("DoThing", [](BT::TreeNode&) {
        return BT::NodeStatus::SUCCESS;
    }, {BT::InputPort<std::string>("message")});
    auto tree = factory.createTreeFromText(kSubTreeXML);
    BT::Groot2Publisher publisher(tree, 17667);
    QTest::qWait(100);

    MainWindow window(GraphicMode::MONITOR, "127.0.0.1", "", "17667");
    auto* monitor = window.findChild<SidepanelMonitor*>();
    QVERIFY(monitor);
    bool connected = false;
    int status_updates = 0;
    bool saw_success = false;
    QObject::connect(monitor, &SidepanelMonitor::connectionUpdate,
                     [&](bool value) { connected = value; });
    QObject::connect(monitor, &SidepanelMonitor::changeNodeStyle,
                     [&](const QString&, const auto& statuses) {
                         if (!statuses.empty()) ++status_updates;
                         for (const auto& entry : statuses)
                             saw_success |= entry.second == NodeStatus::SUCCESS;
                     });
    monitor->on_Connect();
    QVERIFY(connected);
    QVERIFY(window.getTabByName("BehaviorTree"));
    QCOMPARE(tree.tickOnce(), BT::NodeStatus::SUCCESS);
    for (int i = 0; i < 10; ++i)
        QVERIFY(QMetaObject::invokeMethod(monitor, "on_timer", Qt::DirectConnection));
    QVERIFY(status_updates >= 2);
    QVERIFY(saw_success);
    monitor->on_Connect();
    QVERIFY(!connected);
}

void V4CompatTest::desktopWorkflow()
{
    QFile stylesheet(":/stylesheet.qss");
    QVERIFY(stylesheet.open(QIODevice::ReadOnly));
    qApp->setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    const QString screenshot_dir = qEnvironmentVariable("GROOT_SCREENSHOT_DIR", "/tmp");
    const QString sample_path = qEnvironmentVariable("GROOT_SAMPLE_TREE");
    QVERIFY2(!sample_path.isEmpty(), "Set GROOT_SAMPLE_TREE to a v4 XML file");
    QFile sample(sample_path);
    QVERIFY(sample.open(QIODevice::ReadOnly));

    {
        MainWindow editor(GraphicMode::EDITOR);
        editor.resize(1280, 820);
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor, 3000));
        editor.loadFromXML(QString::fromUtf8(sample.readAll()));
        QVERIFY(editor.getTabByName("MainTree"));
        auto tree = BuildTreeFromScene(editor.getTabByName("MainTree")->scene());
        QCOMPARE(tree.nodesCount(), size_t(4));
        auto* palette = editor.findChild<QTreeWidget*>("paletteTreeWidget");
        QVERIFY(palette);
        auto battery_items = palette->findItems("CheckBattery", Qt::MatchExactly | Qt::MatchRecursive);
        QCOMPARE(battery_items.size(), 1);
        QCOMPARE(battery_items.front()->parent()->text(0), QString("Unspecified"));
        QTest::qWait(150);
        QVERIFY(editor.grab().save(screenshot_dir + "/groot-v4-editor.png"));
    }

    {
        MainWindow editor(GraphicMode::EDITOR);
        editor.resize(1280, 820);
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor, 3000));
        editor.loadFromXML(kSubTreeXML);
        auto tree = BuildTreeFromScene(editor.getTabByName("MainTree")->scene());
        const auto* subtree_node = tree.findFirstNode("Worker");
        QVERIFY(subtree_node);
        auto* subtree_model = dynamic_cast<SubtreeNodeModel*>(
            subtree_node->graphic_node->nodeDataModel());
        QVERIFY(subtree_model);
        subtree_model->expandButton()->click();
        QVERIFY(subtree_model->expanded());
        QTest::qWait(150);
        QVERIFY(editor.grab().save(screenshot_dir + "/groot-v4-subtree.png"));
    }

    BT::BehaviorTreeFactory factory;
    factory.registerSimpleAction("DoThing", [](BT::TreeNode&) {
        return BT::NodeStatus::SUCCESS;
    }, {BT::InputPort<std::string>("message")});
    auto tree = factory.createTreeFromText(kSubTreeXML);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString log_path = directory.filePath("desktop.btlog");
    {
        BT::FileLogger2 logger(tree, log_path.toStdString());
        QCOMPARE(tree.tickOnce(), BT::NodeStatus::SUCCESS);
        logger.flush();
    }
    QFile log(log_path);
    QVERIFY(log.open(QIODevice::ReadOnly));

    {
        MainWindow replay_window(GraphicMode::REPLAY);
        replay_window.resize(1280, 820);
        replay_window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&replay_window, 3000));
        auto* replay = replay_window.findChild<SidepanelReplay*>();
        QVERIFY(replay);
        replay->loadLog(log.readAll());
        QVERIFY(replay->transitionsCount() > 0);
        auto* table = replay->findChild<QTableView*>();
        QVERIFY(table);
        QCOMPARE(size_t(table->model()->rowCount()), replay->transitionsCount());
        auto* slider = replay->findChild<QSlider*>("timeSlider");
        QVERIFY(slider);
        auto* spinbox = replay->findChild<QSpinBox*>("spinBox");
        QVERIFY(spinbox);
        QCOMPARE(spinbox->value(), 1);
        slider->setValue(slider->maximum());
        QCOMPARE(spinbox->value(), slider->maximum() + 1);
        QCOMPARE(spinbox->maximum(), slider->maximum() + 1);
        QTest::qWait(150);
        QVERIFY(replay_window.grab().save(screenshot_dir + "/groot-v4-replay.png"));
    }

    {
        BT::Groot2Publisher publisher(tree, 17668);
        QTest::qWait(100);
        MainWindow monitor_window(GraphicMode::MONITOR, "127.0.0.1", "", "17668");
        monitor_window.resize(1280, 820);
        monitor_window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&monitor_window, 3000));
        auto* monitor = monitor_window.findChild<SidepanelMonitor*>();
        QVERIFY(monitor);
        bool connected = false;
        int updates = 0;
        bool saw_success = false;
        QObject::connect(monitor, &SidepanelMonitor::connectionUpdate,
                         [&](bool value) { connected = value; });
        QObject::connect(monitor, &SidepanelMonitor::changeNodeStyle,
                         [&](const QString&, const auto& statuses) {
                             if (!statuses.empty()) ++updates;
                             for (const auto& entry : statuses)
                                 saw_success |= entry.second == NodeStatus::SUCCESS;
                         });
        monitor->on_Connect();
        QVERIFY(connected);
        QCOMPARE(tree.tickOnce(), BT::NodeStatus::SUCCESS);
        QTRY_VERIFY_WITH_TIMEOUT(updates > 1 && saw_success, 2000);
        QVERIFY(monitor_window.getTabByName("BehaviorTree"));
        QVERIFY(monitor_window.grab().save(screenshot_dir + "/groot-v4-monitor.png"));
        monitor->on_Connect();
    }
}

QTEST_MAIN(V4CompatTest)
#include "v4_compat_test.moc"
