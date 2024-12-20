#pragma once
#include "OscBundle.hpp"
#include "oscpack/ip/UdpSocket.h"
#include "oscpack/osc/OscOutboundPacketStream.h"
#include "oscpack/osc/OscTypes.h"

/*
This file was originally copied from https://github.com/The-Modular-Mind/oscelot.

Modifications:

* Removed some FATAL logging, and changed others to WARN

*/

namespace TheModularMind {

class OscSender {
   public:
	std::string host;
	int port = 0;

	OscSender() {}

	~OscSender() { stop(); }

	bool start(std::string &host, int port) {
		this->host = host;
		this->port = port;
		if (host == "") {
			host = "localhost";
		}

		UdpTransmitSocket *socket = nullptr;
		try {
			IpEndpointName name = IpEndpointName(host.c_str(), port);
			if (!name.address) {
				WARN("Bad hostname: %s", host.c_str());
				return false;
			}
			socket = new UdpTransmitSocket(name);
			sendSocket.reset(socket);

		} catch (std::exception &e) {
			WARN("OscSender couldn't start with %s:%i because of: %s", host.c_str(), port, e.what());
			if (socket != nullptr) {
				delete socket;
				socket = nullptr;
			}
			sendSocket.reset();
			return false;
		}
		return true;
	}

	void stop() { sendSocket.reset(); }

	bool hasSocket() {
		if (!sendSocket) {
			return false;
		} else {
			return true;
		}
	}

	void sendBundle(const OscBundle &bundle) {
		if (!sendSocket) {
			return;
		}

		static const int OUTPUT_BUFFER_SIZE = 327680;
		char buffer[OUTPUT_BUFFER_SIZE];
		osc::OutboundPacketStream outputStream(buffer, OUTPUT_BUFFER_SIZE);
		appendBundle(bundle, outputStream);
		sendSocket->Send(outputStream.Data(), outputStream.Size());
	}

	void sendMessage(const OscMessage &message) {
		if (!sendSocket) {
			return;
		}

		static const int OUTPUT_BUFFER_SIZE = 327680;
		char buffer[OUTPUT_BUFFER_SIZE];
		osc::OutboundPacketStream outputStream(buffer, OUTPUT_BUFFER_SIZE);
		appendMessage(message, outputStream);
		sendSocket->Send(outputStream.Data(), outputStream.Size());
	}

   private:
	std::unique_ptr<UdpTransmitSocket> sendSocket;

	void appendBundle(const OscBundle &bundle, osc::OutboundPacketStream &outputStream) {
		outputStream << osc::BeginBundleImmediate;
		for (int i = 0; i < bundle.getBundleCount(); i++) {
			appendBundle(bundle.getBundleAt(i), outputStream);
		}
		for (int i = 0; i < bundle.getMessageCount(); i++) {
			appendMessage(bundle.getMessageAt(i), outputStream);
		}
		outputStream << osc::EndBundle;
	}

	void appendMessage(const OscMessage &message, osc::OutboundPacketStream &outputStream) {
		outputStream << osc::BeginMessage(message.getAddress().c_str());
		for (size_t i = 0; i < message.getNumArgs(); ++i) {
			switch (message.getArgType(i)) {
			case osc::INT32_TYPE_TAG:
				outputStream << message.getArgAsInt(i);
				break;
			case osc::FLOAT_TYPE_TAG:
				outputStream << message.getArgAsFloat(i);
				break;
			case osc::STRING_TYPE_TAG:
				outputStream << message.getArgAsString(i).c_str();
				break;
			default:
				WARN("OscSender.appendMessage(), Unimplemented type?: %i", (int)message.getArgType(i));
				break;
			}
		}
		outputStream << osc::EndMessage;
	}
};
}  // namespace TheModularMind