set(MANUAL_VERSION_PATTERN "__TB_VERSION__")
set(MANUAL_BUILD_ID_PATTERN "__TB_BUILD_ID__")

file(READ "${INPUT}" MANUAL_CONTENTS)
string(REGEX REPLACE "${MANUAL_VERSION_PATTERN}" "${APP_VERSION_YEAR}.${APP_VERSION_NUMBER}" MANUAL_CONTENTS "${MANUAL_CONTENTS}")
string(REGEX REPLACE "${MANUAL_BUILD_ID_PATTERN}" "${GIT_DESCRIBE} ${APP_BUILD_TYPE}" MANUAL_CONTENTS "${MANUAL_CONTENTS}")
file(WRITE "${OUTPUT}" "${MANUAL_CONTENTS}")
