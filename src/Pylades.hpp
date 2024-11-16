#pragma once
#include "plugin.hpp"
#include "RSBATechModules.hpp"
#include "osc/OscSender.hpp"
#include "osc/OscReceiver.hpp"
#include "osc/OscController.hpp"

namespace RSBATechModules {
namespace Pylades {

static const std::string RXPORT_DEFAULT = "8881";
static const std::string TXPORT_DEFAULT = "8880";
static const std::string OSCMSG_FADER = "/fader";
static const std::string OSCMSG_MODULE_START = "/pylades/moduleMeowMory/start";
static const std::string OSCMSG_MODULE_END = "/pylades/moduleMeowMory/end";
static const std::string OSCMSG_PREV_MODULE = "/pylades/prev";
static const std::string OSCMSG_NEXT_MODULE = "/pylades/next";
static const std::string OSCMSG_SELECT_MODULE = "/pylades/select";
static const std::string OSCMSG_LIST_MODULES = "/pylades/listmodules";
static const std::string OSCMSG_RESET_PARAM = "/pylades/resetparam";
static const std::string OSCMSG_RESEND = "/pylades/resend";
static const std::string OSCMSG_APPLY_MODULE = "/pylades/apply/modulemapping";
static const std::string OSCMSG_APPLY_RACK_MAPPING = "/pylades/apply/rackmapping";
static const std::string OSCMSG_VERSION_POLL = "/pylades/version";



} // namespace OrestesOne
} // namespace Orestes