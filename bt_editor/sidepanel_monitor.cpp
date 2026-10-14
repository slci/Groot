#include "sidepanel_monitor.h"
#include "ui_sidepanel_monitor.h"

#include <QDomDocument>
#include <QMessageBox>
#include <QTimer>
#include <cstring>

#include "mainwindow.h"
#include "utils.h"

SidepanelMonitor::SidepanelMonitor(QWidget* parent, const QString& address,
                                   const QString& publisher_port,
                                   const QString& server_port)
    : QFrame(parent), ui(new Ui::SidepanelMonitor),
      _zmq_context(1), _zmq_client(_zmq_context, ZMQ_REQ),
      _connected(false), _msg_count(0), _parent(parent)
{
    Q_UNUSED(publisher_port);
    ui->setupUi(this);
    ui->label_publisher->hide();
    ui->lineEdit_publisher->hide();
    if (!address.isEmpty()) ui->lineEdit_address->setText(address);
    if (!server_port.isEmpty()) ui->lineEdit_server->setText(server_port);
    _load_tree_timeout_ms = _load_tree_default_timeout_ms;
    _zmq_client.set(zmq::sockopt::linger, 0);
    _timer = new QTimer(this);
    connect(_timer, &QTimer::timeout, this, &SidepanelMonitor::on_timer);
}

SidepanelMonitor::~SidepanelMonitor()
{
    _timer->stop();
    stopRecording();
    _zmq_client.close();
    delete ui;
}

void SidepanelMonitor::clear()
{
    if (_connected) disconnect();
}

bool SidepanelMonitor::request(BT::Monitor::RequestType type, std::string& payload,
                               BT::Monitor::TreeUniqueUUID& tree_id,
                               const std::string& extra_part)
{
    try {
        BT::Monitor::RequestHeader header(type);
        zmq::multipart_t outgoing;
        outgoing.addstr(BT::Monitor::SerializeHeader(header));
        if (!extra_part.empty()) outgoing.addstr(extra_part);
        if (!outgoing.send(_zmq_client)) return false;

        zmq::multipart_t incoming;
        if (!incoming.recv(_zmq_client) || incoming.size() < 1 || incoming.size() > 2 ||
            incoming[0].size() != BT::Monitor::ReplyHeader::size()) return false;
        const auto reply = BT::Monitor::DeserializeReplyHeader(incoming[0].to_string());
        if (reply.request != header || reply.request.protocol != BT::Monitor::kProtocolID)
            return false;
        tree_id = reply.tree_id;
        payload = incoming.size() == 2 ? incoming[1].to_string() : std::string();
        return true;
    } catch (const zmq::error_t&) {
        return false;
    }
}

bool SidepanelMonitor::getTreeFromServer()
{
    std::string xml;
    BT::Monitor::TreeUniqueUUID tree_id;
    _zmq_client.set(zmq::sockopt::rcvtimeo, _load_tree_timeout_ms);
    _zmq_client.set(zmq::sockopt::sndtimeo, _load_tree_timeout_ms);
    if (!request(BT::Monitor::FULLTREE, xml, tree_id)) return false;

    QDomDocument document;
    QString error;
    int line = 0;
    if (!document.setContent(QString::fromUtf8(xml.data(), int(xml.size())),
                             &error, &line)) return false;
    try {
        auto result = BuildTreeFromRuntimeXML(document);
        _loaded_tree = std::move(result.first);
        _uid_to_index = std::move(result.second);
        _tree_id = tree_id;
        for (const auto& node : _loaded_tree.nodes()) {
            if (BuiltinNodeModels().count(node.model.registration_ID) == 0)
                emit addNewModel(node.model);
        }
        emit loadBehaviorTree(_loaded_tree, "BehaviorTree");
    } catch (const std::exception&) {
        return false;
    }
    _zmq_client.set(zmq::sockopt::rcvtimeo, 100);
    _zmq_client.set(zmq::sockopt::sndtimeo, 100);
    std::string response;
    BT::Monitor::TreeUniqueUUID response_tree_id;
    if (!request(BT::Monitor::TOGGLE_RECORDING, response, response_tree_id, "start") ||
        response_tree_id != _tree_id) return false;
    _recording = true;
    return getStatusFromServer();
}

bool SidepanelMonitor::getStatusFromServer()
{
    std::string bytes;
    BT::Monitor::TreeUniqueUUID tree_id;
    if (!request(BT::Monitor::STATUS, bytes, tree_id) ||
        tree_id != _tree_id || bytes.size() % 3 != 0) return false;
    std::vector<std::pair<int, NodeStatus>> statuses;
    statuses.reserve(bytes.size() / 3);
    for (size_t offset = 0; offset < bytes.size(); offset += 3) {
        uint16_t uid = 0;
        std::memcpy(&uid, bytes.data() + offset, sizeof(uid));
        const auto found = _uid_to_index.find(uid);
        if (found == _uid_to_index.end()) return false;
        const auto raw = static_cast<uint8_t>(bytes[offset + 2]);
        if (raw > 10 + static_cast<uint8_t>(NodeStatus::SKIPPED)) return false;
        const auto status = raw >= 10 ? NodeStatus::IDLE : static_cast<NodeStatus>(raw);
        if (raw >= 10 && raw != 10)
            statuses.emplace_back(found->second, static_cast<NodeStatus>(raw - 10));
        _loaded_tree.node(found->second)->status = status;
        statuses.emplace_back(found->second, status);
    }
    emit changeNodeStyle("BehaviorTree", statuses);
    dynamic_cast<MainWindow*>(_parent)->lockEditing(true);
    return true;
}

void SidepanelMonitor::on_timer()
{
    if (!_connected) return;
    std::string bytes;
    BT::Monitor::TreeUniqueUUID tree_id;
    if (!request(BT::Monitor::GET_TRANSITIONS, bytes, tree_id)) {
        disconnect();
        return;
    }
    if (tree_id != _tree_id) {
        if (!getTreeFromServer()) disconnect();
        return;
    }
    if (bytes.size() % 9 != 0) {
        disconnect();
        return;
    }
    std::vector<std::pair<int, NodeStatus>> statuses;
    statuses.reserve(bytes.size() / 9);
    for (size_t offset = 0; offset < bytes.size(); offset += 9) {
        uint16_t uid = 0;
        std::memcpy(&uid, bytes.data() + offset + 6, sizeof(uid));
        const auto found = _uid_to_index.find(uid);
        if (found == _uid_to_index.end()) {
            if (!getTreeFromServer()) disconnect();
            return;
        }
        const auto raw = static_cast<uint8_t>(bytes[offset + 8]);
        if (raw > static_cast<uint8_t>(NodeStatus::SKIPPED)) {
            disconnect();
            return;
        }
        const auto status = static_cast<NodeStatus>(raw);
        _loaded_tree.node(found->second)->status = status;
        statuses.emplace_back(found->second, status);
    }
    ui->labelCount->setText(QString("Transition polls: %1").arg(++_msg_count));
    if (!statuses.empty()) emit changeNodeStyle("BehaviorTree", statuses);
    dynamic_cast<MainWindow*>(_parent)->lockEditing(true);
}

void SidepanelMonitor::stopRecording()
{
    if (!_recording) return;
    std::string response;
    BT::Monitor::TreeUniqueUUID tree_id;
    request(BT::Monitor::TOGGLE_RECORDING, response, tree_id, "stop");
    _recording = false;
}

void SidepanelMonitor::disconnect()
{
    _timer->stop();
    stopRecording();
    _connected = false;
    ui->lineEdit_address->setEnabled(true);
    ui->lineEdit_server->setEnabled(true);
    _zmq_client.close();
    _zmq_client = zmq::socket_t(_zmq_context, ZMQ_REQ);
    _zmq_client.set(zmq::sockopt::linger, 0);
    emit connectionUpdate(false);
}

void SidepanelMonitor::on_Connect()
{
    if (_connected) {
        disconnect();
        return;
    }
    QString address = ui->lineEdit_address->text();
    if (address.isEmpty()) address = ui->lineEdit_address->placeholderText();
    QString port = ui->lineEdit_server->text();
    if (port.isEmpty()) port = ui->lineEdit_server->placeholderText();
    ui->lineEdit_address->setText(address);
    ui->lineEdit_server->setText(port);
    _connection_address_req = "tcp://" + address.toStdString() + ":" + port.toStdString();
    _msg_count = 0;
    try {
        _zmq_client.connect(_connection_address_req);
        if (getTreeFromServer()) {
            _connected = true;
            ui->lineEdit_address->setEnabled(false);
            ui->lineEdit_server->setEnabled(false);
            _timer->start(_timer_period_ms);
            emit connectionUpdate(true);
            return;
        }
    } catch (const zmq::error_t&) {
    }
    disconnect();
    QMessageBox::warning(this, tr("ZeroMQ connection"),
                         tr("Could not connect to %1").arg(QString::fromStdString(_connection_address_req)));
}
