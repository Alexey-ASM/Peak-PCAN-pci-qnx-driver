#include <iostream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>

#include <sys/neutrino.h>

#include <fcntl.h>
#include <unistd.h>

#include <can.h>

void PrintUsage(const char* progname)
{
    std::cout << progname << " - send CAN-frames via CAN_RAW sockets.\n\n"
        << "Usage: " << progname << " <device> <can_frame>.\n\n"
        << "<can_frame>:\n"
        << " <can_id>#{data}          for CAN CC (Classical CAN 2.0B) data frames\n"
        << " <can_id>#R{len}          for CAN CC (Classical CAN 2.0B) data frames\n"
        << "<can_id>:\n"
        << " 3 (SFF) or 8 (EFF) hex chars\n"
        << "{data}:\n"
        << " 0..8 ASCII hex-values (optionally separated by '.')\n"
        << "{len}:\n"
        << " an optional 0..8 value as RTR frames can contain a valid dlc field\n"
        << "Examples:\n"
        << "  5A1#11.2233.44556677.88 / 123#DEADBEEF / 5AA# /\n"
        << "  1F334455#1122334455667788 / 123#R / 00000123#R3 / 333#R8 /\n"
        << std::endl;
}

bool IsHexChar(char c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

bool ParseCanFrame(const std::string& str, can_frame& frame)
{
	auto separatorPos = str.find("#");

	if(separatorPos == std::string::npos || separatorPos > 8)
	{
		return false;
	}

	std::istringstream is(str);

	char separator;
	std::string data;

	is >> std::hex >> frame.can_id >> separator >> data;

	if(separator != '#')
	{
		return false;
	}

	if(separatorPos > 3 || frame.can_id > 0x7ff)
	{
		frame.can_id |= CAN_EFF_FLAG;
	}

	if(!data.empty())
	{
		if(data[0] == 'r' || data[0] == 'R')
		{
			switch(data.size())
			{
				case 1:
					frame.len = 0;
				break;
				case 2:
					if(data[1] >= '0' && data[1] <= '8')
					{
						frame.len = data[1] - '0';
					}
					else
					{
						return false;
					}
				break;
				default:
					return false;
			}

			frame.can_id |= CAN_RTR_FLAG;
		}
		else
		{
			std::vector<std::string> values;
			std::stringstream ss(data);
			std::string token;

			// Split by '.'
			while (std::getline(ss, token, '.'))
			{
				if (token.empty())
				{
					return false;
				}
				if (token.length() <= 2)
				{
					// Directly add 1 or 2 character tokens
					values.push_back(token);
				} else {
					// Split into 2-character substrings
					for (size_t i = 0; i < token.length(); i += 2)
					{
						if (i + 2 <= token.length())
						{
							values.push_back(token.substr(i, 2));
						} else {
							// Last character if odd length
							values.push_back(token.substr(i, 1));
						}
					}
				}
			}

			if(values.size() > 8)
			{
				return false;
			}

			for(size_t i = 0; i < values.size(); ++i)
			{
				if(IsHexChar(values[i][0]) && ((values[i].size() == 1) || IsHexChar(values[i][1])))
				{
					frame.data[i] = std::stoi(values[i], 0, 16);
					std::cout << std::dec << int(frame.data[i]) << std::endl;
				}
				else
				{
					return false;
				}
			}

			frame.len = values.size();
		}
	}

	return true;
}

int main(int argc, char **argv)
{
	/* check command line options */
	if (argc != 3) {
		PrintUsage(argv[0]);
		return 1;
	}

	can_frame canFrame;

	if(!ParseCanFrame(argv[2], canFrame))
	{
		PrintUsage(argv[0]);
		return -1;
	}

	int canController = open("/dev/can0", O_RDWR | O_APPEND);

	if (-1 == canController)
	{
		std::cout << "open " << "/dev/can0" << " controller error " << std::endl;
		return -1;
	}

	/* send frame */
	const auto bytesWritten = write(canController, &canFrame, sizeof(can_frame));

	if (bytesWritten != sizeof(can_frame))
	{
		std::cerr << "Can not write message to the can controller" << std::endl;
	}

	close(canController);

	return 0;
}
