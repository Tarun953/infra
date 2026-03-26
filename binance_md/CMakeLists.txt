add_executable(binance_market_data
    src/binance_market_data.cpp
)

target_link_libraries(binance_market_data
    PRIVATE common
)