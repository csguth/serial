#include <array>
#include <boost/asio/serial_port.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/optional.hpp>
#include <chrono>
#include <iostream>
#include <numeric>
#include <thread>
#include <regex>
#include <boost/date_time/local_time/local_time.hpp>


namespace asio = boost::asio;

std::vector<std::string> read_as_lines(std::string input)
{
    std::vector<std::string> lines;
    std::stringstream iss(std::move(input));
    while (iss.good())
    {
        std::string temp;
        std::getline(iss, temp, '\n');
        lines.push_back(std::move(temp));
    }
    return lines;
}

std::vector<std::array<int, 2>> parse_as_sensor_data(std::vector<std::string> data)
{
    std::vector<std::array<int, 2>> sensors;
    std::transform(data.begin(), data.end(), std::back_inserter(sensors), [](std::string element) -> std::array<int, 2> {
        auto position = element.find_first_of(',');
        if (position >= element.size() - 1 || position == 0)
        {
            return std::array<int, 2>{{-1, -1}};
        }
        return std::array<int, 2>{{
            std::stoi(element.substr(0, element.find_first_of(','))),
            std::stoi(element.substr(element.find_first_of(',')+1))
        }};
    });
    return sensors;
}

std::array<int, 2> smart_average(std::vector<std::array<int, 2>> data)
{
    std::array<int, 2> result{{ -1, -1}};
    
    int tenpct = data.size() * .1;
    if (tenpct > 0)
    {
        auto newbegin = std::next(data.begin(), tenpct);
        auto newend = std::prev(data.end(), tenpct);
        
        result = std::accumulate(newbegin, newend, std::array<int, 2>{{0, 0}}, [](const std::array<int, 2>& prev, std::array<int, 2> val){
            return std::array<int, 2>{{prev[0]+val[0], prev[1]+val[1]}};
        });
        
        result[0] /= std::distance(newbegin, newend);
        result[1] /= std::distance(newbegin, newend);
    }
    
    return result;
}

int INTERVAL = 5;

void handle_read(asio::serial_port& port, char* buffer, std::size_t bytes_read, boost::system::error_code ec)
{
    if (bytes_read == 0)
    {
        return;
    }
    
    std::string data{buffer, bytes_read};
    auto lines = read_as_lines(std::move(data));
    auto sensors = parse_as_sensor_data(std::move(lines));
    auto avg = smart_average(std::move(sensors));
    
    static decltype(boost::posix_time::second_clock::local_time()) timeSinceLastOutput;
    auto currentDate = boost::posix_time::second_clock::local_time();
    boost::posix_time::time_duration td = currentDate - timeSinceLastOutput;
    if (td.total_seconds() > INTERVAL)
    {
        std::cout << currentDate << "," << avg[0] << "," << avg[1] << std::endl;
        timeSinceLastOutput = currentDate;
    }
    
    asio::async_read(port, asio::buffer(buffer, 1024), boost::bind(handle_read, boost::ref(port), buffer, asio::placeholders::bytes_transferred, asio::placeholders::error));
}


int main(int argc, char* argv[])
{
    asio::io_service io;
    boost::optional<asio::io_service::work> w{io};
    auto t = std::thread([&io](){
        io.run();
    });
    asio::serial_port port(io);
    
    if (argc == 3)
    {
        INTERVAL = std::stoi(argv[2]);
    }

    try {
        port.open(argv[1]);
    }
    catch(std::exception& exc)
    {
        std::cerr << "error trying to read " << argv[1] << std::endl;
        return -1;
    }
    port.set_option(asio::serial_port_base::baud_rate(9600));
    
    std::array<char, 1024> buff;
    
    asio::async_read(port, asio::buffer(buff), boost::bind(handle_read, boost::ref(port), buff.data(), asio::placeholders::bytes_transferred, asio::placeholders::error));
    
    w = boost::none;
    t.join();
    return 0;
}
