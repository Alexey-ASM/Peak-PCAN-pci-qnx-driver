#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <vector>
#include <fstream>

#include <sys/neutrino.h>

#include <fcntl.h>
#include <unistd.h>

#include <can.h>

#define SILENT_INI 42 /* detect user setting on commandline */
#define SILENT_OFF 0 /* no silent mode */
#define SILENT_ON 1 /* silent mode (completely silent) */

#define SWAP_DELIMITER '`'

std::chrono::steady_clock::time_point lastTp;

static char* progname;

unsigned char timeStamp = 0;
unsigned char logTimeStamp = 'a';
unsigned char useNs = 0;

extern int optind;

void PrintUsage(void)
{
	std::cout << progname << " - dump CAN bus traffic." << std::endl;
	std::cout << std::endl << "Usage: " << progname << " [options] <CAN interface>+" << std::endl;
	std::cout << "  (use CTRL-C to terminate " << progname << ")" << std::endl << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "         -t <type>   (timestamp: (a)bsolute/(d)elta/(z)ero/(A)bsolute w date)" << std::endl;
	std::cout << "         -N          (log nanosecond timestamps instead of microseconds)" << std::endl;
	std::cout << "         -a          (enable additional ASCII output)" << std::endl;
	std::cout << "         -s          (silent mode)" << std::endl;
	std::cout << "         -l          (log CAN-frames into file. Sets '-s (silent mode) by default)" << std::endl;
	std::cout << "         -f <fname>  (log CAN-frames into file <fname>. Sets '-s "<< SILENT_ON << "' by default)" << std::endl;
	std::cout << "         -n <count>  (terminate after reception of <count> CAN frames)" << std::endl;
	std::cout << "         -e          (dump CAN error frames in human-readable format)" << std::endl;
	std::cout << "         -T <msecs>  (terminate after <msecs> if no frames were received)" << std::endl;
	std::cout << std::endl;
	std::cout << "One CAN interfaces with optional filter sets can be specified" << std::endl;
	std::cout << "on the commandline in the form: <ifname>[,filter]*" << std::endl;
	std::cout << std::endl << "Filters:" << std::endl;
	std::cout << "  Comma separated filters can be specified for each given CAN interface:" << std::endl;
	std::cout << "    <can_id>:<can_mask>" << std::endl << "         (matches when <received_can_id> & mask == can_id & mask)" << std::endl;
	std::cout << "    <can_id>~<can_mask>" << std::endl << "         (matches when <received_can_id> & mask != can_id & mask)" << std::endl;
	std::cout << std::endl << "CAN IDs, masks and data content are given and expected in hexadecimal values." << std::endl;
	std::cout << "When the can_id is 8 digits long the CAN_EFF_FLAG is set for 29 bit EFF format." << std::endl;
	std::cout << "Without any given filter all data frames are received ('0:0' default filter)." << std::endl;
	std::cout << std::endl << "Examples:" << std::endl;
	std::cout << progname << " -ta can0,123:7FF,400:700" << std::endl << std::endl;
	std::cout << progname << " vcan2,12345678:DFFFFFFF" << std::endl << "         (match only for extended CAN ID 12345678)" << std::endl;
	std::cout << progname << " vcan2,123:7FF" << std::endl << "         (matches CAN ID 123 - including EFF and RTR frames)" << std::endl;
	std::cout << progname << " vcan2,123:C00007FF" << std::endl << "         (matches CAN ID 123 - only SFF and non-RTR frames)" << std::endl;
	std::cout << std::endl;
}

std::ostream& SprintTimestamp(std::ostream& os)
{
	switch (timeStamp) {
	case 'a': /* absolute with timestamp */
	{
		const auto now = std::chrono::system_clock::now();
		const auto duration = now.time_since_epoch();

		const auto sec = std::chrono::duration_cast<std::chrono::seconds>(duration);
		os << std::setw(10) << std::setfill('0') << sec.count() << ".";

		if (useNs)
		{
		    const auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - sec);
		    os << std::setw(9) << std::setfill('0') << nsec.count();
		} else {
		    const auto usec = std::chrono::duration_cast<std::chrono::microseconds>(duration - sec);
		    os << std::setw(6) << std::setfill('0') << usec.count();
		}
		break;
	}
	case 'A': /* absolute with date */
	{
		const auto now = std::chrono::system_clock::now();
		const auto time_t = std::chrono::system_clock::to_time_t(now);
		std::tm tm = *std::localtime(&time_t);

		os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << ".";

		const auto duration = now.time_since_epoch();
		const auto sec = std::chrono::duration_cast<std::chrono::seconds>(duration);

		if (useNs)
		{
			const auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - sec);
			os << std::setw(9) << std::setfill('0') << nsec.count();
		} else {
		    const auto usec = std::chrono::duration_cast<std::chrono::microseconds>(duration - sec);
		    os << std::setw(6) << std::setfill('0') << usec.count();
		}
	}
	break;

	case 'd': /* delta */
	case 'z': /* starting with zero */
	{
		const auto now = std::chrono::steady_clock::now();

		if (lastTp == std::chrono::steady_clock::time_point()) /* first init */
			lastTp = now;

		const auto duration = now - lastTp;

		const auto sec = std::chrono::duration_cast<std::chrono::seconds>(duration);
		os << std::setw(10) << std::setfill('0') << sec.count() << ".";

		if (useNs)
		{
		    const auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - sec);
		    os << std::setw(9) << std::setfill('0') << nsec.count();
		} else {
		    const auto usec = std::chrono::duration_cast<std::chrono::microseconds>(duration - sec);
		    os << std::setw(6) << std::setfill('0') << usec.count();
		}

		if (timeStamp == 'd')
			lastTp = now;
	}
	break;

	default: /* no timestamp output */
		break;
	}

	return os;
}

std::vector<std::string> SplitString(const std::string& input)
{
    std::vector<std::string> tokens;
    std::istringstream ss(input);
    std::string token;

    while (std::getline(ss, token, ','))
    {
        tokens.push_back(token);
    }

    return tokens;
}

void ParseCanFilter(const std::string& str, std::vector<can_filter>& filters)
{
    std::istringstream iss(str);
    uint32_t can_id, can_mask;
    char separator;

    if (iss >> std::hex >> can_id >> separator >> std::hex >> can_mask)
    {
        if (separator == ':')
        {
        	filters.emplace_back(can_filter{can_id, can_mask & ~CAN_ERR_FLAG});

        	if (str.length() > 8 && str[8] == ':')
            {
        		filters.back().can_id |= CAN_EFF_FLAG;
            }
            return;
        }

        if (separator == '~')
        {
        	filters.emplace_back(can_filter{can_id | CAN_INV_FILTER, can_mask & ~CAN_ERR_FLAG});

        	if (str.length() > 8 && str[8] == '~')
            {
            	filters.back().can_id |= CAN_EFF_FLAG;
            }
        }
    }
}

bool CanFilterPassed(const std::vector<can_filter>& canFilters, const can_frame& message)
{
	for(const auto& filter: canFilters)
	{
		if(filter.can_id & CAN_INV_FILTER)
		{
			if((message.can_id & filter.can_mask) != (filter.can_id & filter.can_mask))
			{
				return true;
			}
		}
		else
		{
			if((message.can_id & filter.can_mask) == (filter.can_id & filter.can_mask))
			{
				return true;
			}
		}
	}

	return false;
}

int main(int argc, char *argv[]) {

	progname = argv[0];

	unsigned char log = 0;
	int count = 0;
	int option = 0;
	std::string logname;
	int asciiView = 0;
	unsigned char silent = SILENT_INI;

	while ((option = getopt(argc, argv, "t:HNciaSs:lf:Ln:r:Dde8xT:h?")) != -1)
	{
		switch (option)
		{
		case 't':
			timeStamp = optarg[0];
			logTimeStamp = optarg[0];
			if ((timeStamp != 'a') && (timeStamp != 'A') &&
			    (timeStamp != 'd') && (timeStamp != 'z'))
			{
				std::cerr << progname << ": unknown timestamp mode '" << optarg[0] << "' - ignored" << std::endl;

				timeStamp = 0;
			}

			if ((logTimeStamp != 'a') && (logTimeStamp != 'z'))
			{
				logTimeStamp = 'a';
			}
			break;

		case 'N':
			useNs = 1;
			break;

		case 'a':
			asciiView = 1;
			break;

		case 's':
			silent = SILENT_ON;
			break;

		case 'l':
			log = 1;
			break;

		case 'f':
			logname = optarg;
			log = 1;
			break;

		case 'n':
			count = atoi(optarg);
			if (count < 1)
			{
				PrintUsage();
				exit(1);
			}
			break;

		default:
			PrintUsage();
			exit(1);
			break;
		}
	}

	if (optind == argc)
	{
		PrintUsage();
		exit(0);
	}

	/* "-f -"  is equal to "-L" (print logfile format on stdout) */
	if (log && !logname.empty() && "-" == logname)
	{
		log = 0; /* no logging into a file */
	}

	if (silent == SILENT_INI)
	{
		if (log)
		{
			std::cerr << "Disabled standard output while logging." << std::endl;
			silent = SILENT_ON; /* disable output on stdout */
		} else {
			silent = SILENT_OFF; /* default output */
		}
	}

	std::ofstream logFile;

	if (log)
	{
		if (logname.empty())
		{
			const auto now = std::chrono::system_clock::now();
			const auto time_t = std::chrono::system_clock::to_time_t(now);
			std::tm tm = *std::localtime(&time_t);

			std::ostringstream os;

			os << std::put_time(&tm, "candump-%Y-%m-%d_%H%M%S.log");

			logname = os.str();
		}

		if (silent != SILENT_ON)
		{
			std::cout << "Warning: Console output active while logging!" << std::endl;
		}


		std::cout << "Enabling Logfile '"<< logname << "'" << std::endl;

		logFile.open(logname);

		if(!logFile)
		{
			std::cout << "logfile open error " << std::endl;
			return 1;
		}
	}

    auto tokens = SplitString(argv[optind]);

	int canController = open((std::string("/dev/") + tokens[0]).c_str(), O_RDWR | O_APPEND);

	std::vector<can_filter> canFilters;

	for(size_t i = 1; i < tokens.size(); ++i)
	{
		ParseCanFilter(tokens[i], canFilters);
	}

    if (-1 == canController)
    {
        std::cout << "open " << tokens[0] << " controller error " << std::endl;;
        return -1;
    }

    while (true)
    {
        can_frame message;

        auto result = read(canController, &message, sizeof(can_frame));

        if (-1 == result)
        {
            std::cout << "read error" << std::endl;
            break;
        }

        if (result != 0 && (canFilters.empty() || CanFilterPassed(canFilters, message)))
        {
        	std::ostringstream os;

        	os << SprintTimestamp << " ";
        	os << tokens[0];

      		os << std::hex << std::setfill(' ') << std::setw(10) << (message.can_id & CAN_EFF_MASK);
      		os << std::dec << std::setfill(' ') << std::setw(3) << int(message.len) << " ";

      		for(int i = 0; i < 8; ++i)
      		{
      			if(message.len <= i)
      			{
      				os	<< "   ";
      			}
      			else
      			{
      				os << " " << std::hex << std::setfill('0') << std::setw(2) << int(message.data[i]);
      			}
      		}

      		if(asciiView)
      		{
      			os << "  ";

      			for(int i = 0; i < message.len; ++i)
          		{
      				if(message.data[i] > 31 && message.data[i] != 127)
      				{
      					os << message.data[i];
      				}
      				else
      				{
      					os << '.';
      				}
          		}
      		}

      		if(silent != SILENT_ON)
      		{
      			std::cout << os.str() << std::endl;
      		}

      		if(log && logFile)
      		{
      			logFile << os.str() << std::endl;
      		}

			if(count && (--count == 0))
			{
				break;
			}
        }
    }

    if (-1 != canController)
    {
        close(canController);
    }

	return EXIT_SUCCESS;
}
