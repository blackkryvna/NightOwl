#include "core/EventQueue.h"

namespace Events {

void registerMetaTypes() {
    qRegisterMetaType<Events::SessionStarted>("Events::SessionStarted");
    qRegisterMetaType<Events::AuthAttempt>("Events::AuthAttempt");
    qRegisterMetaType<Events::SessionFinished>("Events::SessionFinished");
}

} // namespace Events
