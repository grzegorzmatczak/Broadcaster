#include "../include/broadcaster.h"

constexpr auto IP{ "Ip" };
constexpr auto PORT{ "Port" };
constexpr auto PID{ "Pid" };
constexpr auto ID{ "Id" };
constexpr auto COMMAND{ "Command" };
constexpr auto PING{ "Ping" };
constexpr auto MESSAGE_TYPE{ "MessageType" };
constexpr auto TIME{ "Time" };
constexpr auto ERROR_DATA{ "Error" };

Broadcaster::Broadcaster(QJsonObject const& a_config) 
	: m_id{ a_config[ID].toInt() } 
{
	//Logger->set_pattern("[%Y-%m-%d] [%H:%M:%S.%e] [%t] [%^%l%$] %v");
	Logger->info("Broadcaster::Broadcaster()");
	configure(a_config);
}

Broadcaster::~Broadcaster() {}

void Broadcaster::configure(QJsonObject const& a_config) 
	
{
	Logger->trace("Broadcaster::configure()");
	QObject::connect(this, &Broadcaster::subscribeRequest, &m_IO,
		&MQt::onSubscribe);
	QObject::connect(this, &Broadcaster::unsubscribeRequest, &m_IO,
		&MQt::onUnsubscribe);
	QObject::connect(&m_IO, &MQt::newMessage, this, &Broadcaster::onNewMessage);
	QObject::connect(this, &Broadcaster::sendMessageRequest, &m_IO,
		&MQt::onSendMessage);
	QObject::connect(&m_IO, &MQt::connected, this, &Broadcaster::onConnected);
	QObject::connect(&m_IO, &MQt::disconnected, this,
		&Broadcaster::onDisconnected);

	m_ip = a_config[IP].toString();
	m_id = a_config[ID].toInt();
	m_port = static_cast<quint16>(a_config[PORT].toInt());
	onConnect();
	Logger->info("Broadcaster::configure() ip:{} port:{}", m_ip.toStdString(),
		QString::number(m_port).toStdString());
}

void Broadcaster::onConnect() { m_IO.startConnection(m_ip, m_port); }

void Broadcaster::onSubscribeSingleTopic(qint32 topic) {
	Logger->trace("Broadcaster::onSubscribeSingleTopic() topic:{}", topic);
	QVector<qint32> topics{};
	topics.push_back(topic);
	emit(subscribeRequest(topics));
}

void Broadcaster::onSubscribe(QVector<qint32> topics) {
	emit(subscribeRequest(topics));
}

void Broadcaster::onUnsubscribeSingleTopic(qint32 topic) {
	Logger->trace("Broadcaster::onUnsubscribeSingleTopic() topic:{}", topic);
	QVector<qint32> topics{};
	topics.push_back(topic);
	emit(unsubscribeRequest(topics));
}

void Broadcaster::onUnsubscribe(QVector<qint32> topics) {
	emit(unsubscribeRequest(topics));
}

void Broadcaster::onNewMessage(QByteArray const a_message) {
	Logger->trace("Broadcaster::onNewMessage(QByteArray const a_message)");
	MQtMessage message{};
	message.parse(a_message);
	QJsonObject jOut{ {"none", "none"} };
	bool ret = message.parse(a_message);
	if (ret <= 0) {
		Logger->error("Broadcaster::onNewMessage() msg not correct");
	}
	else {
		MQtMessage::Header m_header = message.header();
		Logger->trace("Broadcaster::onNewMessage() a_message:{} m_header:{}", a_message.size() - 20, m_header.size);
		Logger->trace("Broadcaster::onNewMessage() sender:{} topic:{}", message.sender(), message.topic());
		if (message.type() == MQtMessage::BINARY)
		{
			Logger->debug("Recived message that is Message::BINARY");
			QByteArray data = message.content();
			emit(updateImage(data, message.topic()));
			
		}
		if (message.type() == MQtMessage::JSON)
		{
			Logger->debug("Recived msg that is Message::JSON");
			if (message.isValid()) {
				auto jDoc{ QJsonDocument::fromJson(message.content()) };
				if (!jDoc.isObject()) {
					Logger->error("Broadcaster::onNewMessage() Recived invalid  Message::JSON");
				}
				jOut = jDoc.object()[COMMAND].toObject();
				if(jOut[MESSAGE_TYPE].toString() == PING)
				{
					Logger->debug("MQtMessage::JSON is a ping message");
					emit(updatePing(jOut));
				}
				if (jOut[MESSAGE_TYPE].toString() == ERROR_DATA)
				{
					Logger->debug("MQtMessage::JSON is a error data message");
					emit(updateError(jOut));
				}
				else {
					emit(newMessage(jOut));
					QByteArray stateData{ QJsonDocument{jOut}.toJson(QJsonDocument::Compact) };
					Logger->trace("Broadcaster::onNewMessage from:{} \n{}", message.topic(), stateData.toStdString().c_str());
				}
			}
		}
	}
}

void Broadcaster::onSendCommand(const qint32 topic, const QJsonObject json) {
	MQtMessage msg{};
	QByteArray stateData{ QJsonDocument{json}.toJson(QJsonDocument::Compact) };
	msg.fromData(stateData, MQtMessage::JSON, m_id, topic);
	Logger->trace("Broadcaster::onSendCommand to:{} \n{}", topic, stateData.toStdString().c_str());
	emit(sendMessageRequest(msg.rawData()));
}

void Broadcaster::onSendImage(const qint32 topic, QByteArray image) {
	MQtMessage msg{};
	msg.fromData(image, MQtMessage::BINARY, m_id, topic);
	Logger->trace("Broadcaster::onSendImage from {} to:{}", m_id, topic);
	emit(sendMessageRequest(msg.rawData()));
}

void Broadcaster::onSendPing(const qint32 topic) {
	QJsonObject json = { {MESSAGE_TYPE, PING}, {TIME, QDateTime().currentDateTime().toString("hh:mm:ss AP dd/MM/yyyy")} , {ID, m_id} };
	QJsonObject cmd = { {COMMAND, json} };
	MQtMessage msg{};
	QByteArray stateData{ QJsonDocument{cmd}.toJson(QJsonDocument::Compact) };
	msg.fromData(stateData, MQtMessage::JSON, m_id, topic);
	Logger->trace("Broadcaster::onSendPing() from {} to:{}", m_id, topic);
	emit(sendMessageRequest(msg.rawData()));
}

void Broadcaster::onSendError(const qint32 topic, const qint32 error) {
	QJsonObject json = { {MESSAGE_TYPE, ERROR_DATA}, {ERROR_DATA, error} , {ID, m_id} };
	QJsonObject cmd = { {COMMAND, json} };
	MQtMessage msg{};
	QByteArray stateData{ QJsonDocument{cmd}.toJson(QJsonDocument::Compact) };
	msg.fromData(stateData, MQtMessage::JSON, m_id, topic);
	Logger->trace("Broadcaster::onSendError() from {} to:{}", m_id, topic);
	emit(sendMessageRequest(msg.rawData()));
}

void Broadcaster::onConnected() { emit(connected()); }

void Broadcaster::onDisconnected() { emit(disconnected()); }
