include(FetchContent)

FetchContent_Declare(
    fkYAML
    GIT_REPOSITORY https://github.com/fktn-k/fkYAML.git
    GIT_TAG v0.3.12
)

FetchContent_MakeAvailable(fkYAML)