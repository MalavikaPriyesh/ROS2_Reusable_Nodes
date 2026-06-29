std_msgs/Header header
string node_name
string topic_name
uint8 status
uint8 reason
string message
float32 last_update_age_s

# Status Constants
uint8 STATUS_OK = 0
uint8 STATUS_STALE = 1
uint8 STATUS_INACTIVE = 2
uint8 STATUS_UNKNOWN = 3
uint8 STATUS_ERROR = 4

# Reason Constants
uint8 REASON_NONE = 0
uint8 REASON_MAINTENANCE = 1
uint8 REASON_DEREGISTERED = 2
uint8 REASON_OPTIONAL_DISABLED = 3
uint8 REASON_MISSION_NOT_REQUIRED = 4
uint8 REASON_HEARTBEAT_TIMEOUT = 5
uint8 REASON_MESSAGE_TIMEOUT = 6
uint8 REASON_DEADLINE_MISSED = 7
uint8 REASON_QOS_INCOMPATIBLE = 8
uint8 REASON_LIVELINESS_LOST = 9
