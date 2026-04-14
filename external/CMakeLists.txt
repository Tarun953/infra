include(FetchContent)

FetchContent_Declare(
    fkYAML
    GIT_REPOSITORY https://github.com/fktn-k/fkYAML.git
    GIT_TAG v0.3.12
)

FetchContent_Declare(
    quill
    GIT_REPOSITORY https://github.com/odygrd/quill.git
    GIT_TAG v7.3.0 #requires 4+ API for Backend and Frontend separation
)

FetchContent_MakeAvailable(fkYAML)
FetchContent_MakeAvailable(quill)