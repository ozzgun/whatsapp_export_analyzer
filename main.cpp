#include <iostream>
#include <iterator>
#include <string>
#include <sstream>
#include <regex>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <codecvt>

namespace
{
    enum msgStatus
    {
        STATUS_UPDATED,
        STATUS_NEW_ENTRY,
        STATUS_IGNORE
    };

    enum msgType
    {
        TYPE_NORMAL,
        TYPE_LINK,
        TYPE_MEDIA,
        TYPE_IGNORE
    };

    typedef struct msgDetails
    {
        int day;
        int month;
        int year;
        int hour;
        std::string usr_name;
        std::string msg_content;
        msgType     content_type;

        msgDetails(int d, int m, int y, int h, std::string n, std::string c, msgType ct) :
            day(d), month(m), year(y), hour(h), usr_name(n), msg_content(c), content_type(ct)
        {
        }
    }msgDetails;

    typedef struct stats
    {
        int total_messages;
        int nr_of_talked_days;
        int n_media;
        int n_links;
        std::map<int, int> hourly_msg;
        std::map<int, int> messages_per_day;

        stats() : total_messages(0), nr_of_talked_days(0), n_media(0), n_links(0)
        {

        }
    }stats;
    
    using svec          = std::vector<std::string>;
    using msgDetailsVec = std::vector<msgDetails>;
    using usrStatMap    = std::map<std::string, std::map<int, stats>>; //usr - year > <data>
}

namespace
{
    bool is_string_found(const std::string msg, std::string search_string)
    {
        std::regex self_regex(search_string,
            std::regex_constants::ECMAScript | std::regex_constants::icase);
        if (std::regex_search(msg, self_regex))
        {
            return true;
        }
        return false;

    }

    msgType get_msg_type(std::string content)
    {
        if (is_string_found(content, "Media omitted"))
        {
            return TYPE_MEDIA;
        }
        else if (is_string_found(content, "http"))
        {
            return TYPE_LINK;
        }
        else if (is_string_found(content, "You blocked this contact")       ||
                 is_string_found(content, "You unblocked this contact")     ||
                 is_string_found(content, "You created group \"")           ||
                 is_string_found(content, " changed the group description") ||
                 is_string_found(content, " started a video call")          ||
                 is_string_found(content, " started a voice call")          ||
                 is_string_found(content, "Messages and calls are end-to-end encrypted."))
        {
            return TYPE_IGNORE;
        }
        else
        {
            return TYPE_NORMAL;
        }
    }

    int get_day_of_the_year(int day, int month, int year)
    {
        static const int days_in_months[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        int              is_leap_year = year % 4 == 0;

        int nth_day = day + (is_leap_year == 0 ? 0 : (month > 2 ? 1 : 0));
        for (int i = 0; i < month - 1; i++)
        {
            nth_day += days_in_months[i];
        }
        return nth_day;
    }

    svec split_name_and_content(std::string message)
    {
        svec name_content;
        size_t first_occurance = message.find(":");

        name_content.push_back(message.substr(0, first_occurance));
        name_content.push_back(message.substr(first_occurance + 1, message.size()));

        return name_content;
    }

    msgStatus parse_msg(std::string msg, msgDetailsVec& all_messages)
    {
        std::regex  pattern("(\\d{2})/(\\d{2})/(\\d{4}), (\\d{1,2}):\\d{1,2} - (.*)");
        std::smatch results;
        if (std::regex_match(msg, results, pattern))
        {
            svec name_and_content = split_name_and_content(results[5].str());

            std::string name = name_and_content[0];
            std::string content = name_and_content[1];
            msgType     msg_type = get_msg_type(content);

            if (msg_type == TYPE_IGNORE)
            {
                return STATUS_IGNORE;
            }

            msgDetails tmp_msg(std::atoi(results[1].str().c_str()),
                               std::atoi(results[2].str().c_str()),
                               std::atoi(results[3].str().c_str()),
                               std::atoi(results[4].str().c_str()),
                               name, content, msg_type);

            all_messages.push_back(tmp_msg);
            return STATUS_NEW_ENTRY;
        }
        else
        {
            if (all_messages.size() == 0)
            {
                std::cout << "Export started not on a common format. Ignoring the message: " << msg;
                return STATUS_IGNORE;
            }

            const auto new_msg_typ = get_msg_type(msg);
            if (new_msg_typ != TYPE_NORMAL)
            {
                all_messages.back().content_type = new_msg_typ;
            }
            all_messages.back().msg_content.append(" " + msg);
            return STATUS_UPDATED;
        }
    }
}

namespace
{
    void update_stats(const msgDetails& msg_detail, const msgStatus status, usrStatMap& all_stats)
    {
        int year = msg_detail.year;
        std::string usr_name = msg_detail.usr_name;
        if (status == STATUS_UPDATED)
        {
            if      (TYPE_MEDIA == msg_detail.content_type) all_stats[usr_name][year].n_media++;
            else if (TYPE_LINK  == msg_detail.content_type) all_stats[usr_name][year].n_links++;
        }
        else
        {
            int day_of_year = get_day_of_the_year(msg_detail.day, msg_detail.month, msg_detail.year);

            all_stats[usr_name][year].total_messages++;
            all_stats[usr_name][year].hourly_msg[msg_detail.hour]++;
            all_stats[usr_name][year].messages_per_day[day_of_year]++;

            all_stats[usr_name][year].nr_of_talked_days = (int)all_stats[usr_name][year].messages_per_day.size();

            if      (TYPE_MEDIA == msg_detail.content_type) all_stats[usr_name][year].n_media++;
            else if (TYPE_LINK  == msg_detail.content_type) all_stats[usr_name][year].n_links++;
        }
    }

    std::string fetch_date_from_msg_details(msgDetails& msg_detail)
    {
        std::stringstream ss;
        ss << msg_detail.day << "/" << msg_detail.month << "/" << msg_detail.year;
        return ss.str();
    }
}

int main(const int argc, const char* argv[])
{
    if (argc < 2)
    {
        std::cout << "This program must have at least input.\n"\
                     "First input must be full path to a whatsapp export message file.\n"\
                     "An optional output path can be given as input. Otherwise current working dierctory will be used.\n"\
                     "<name>_whatsapp_analysis will be appened to output path to save results";
        return -1;
    }

    msgDetailsVec all_messages;
    usrStatMap    all_stats;

    std::string line;
    std::string chat_path   = argv[1];
    std::string export_path = argc == 3 ? argv[2] : "";

    std::ifstream chat_file(chat_path);
    if(chat_file.is_open())
    {
        while (std::getline(chat_file, line))
        {
            msgStatus status = parse_msg(line, all_messages);
            if (status != STATUS_IGNORE)
            {
                update_stats(all_messages.back(), status, all_stats);
            }
        }
    }

    //Export statistics
    {
        for (const auto person : all_stats)
        {
            std::string export_person_path = export_path + person.first + "_whatsapp_analysis" + ".csv";
            std::ofstream export_file(export_person_path);

            if (export_file.is_open())
            {
                export_file << "This data represents messages sent from: " << person.first << std::endl;
                export_file << "Analyzed messages are between these dates: " << fetch_date_from_msg_details(all_messages[0]) << " and ";
                export_file << fetch_date_from_msg_details(all_messages.back()) << ". The data is analiyzed per year base." << std::endl;

                for (const auto year : person.second)
                {
                    export_file << std::endl << "The data showns for year: " << year.first << std::endl << std::endl;

                    const auto buf = year.second;
                    export_file << "Name;"                    << person.first          << std::endl;
                    export_file << "Total sent messages;"     << buf.total_messages    << std::endl;
                    export_file << "Number of talked days;"   << buf.nr_of_talked_days << std::endl;
                    export_file << "Average message per day;" << round(buf.total_messages / buf.nr_of_talked_days) << std::endl;
                    export_file << "Number of media shared;"  << buf.n_media << std::endl;
                    export_file << "Number of links shared;"  << buf.n_links << std::endl;

                    export_file << std::endl << "Messages Per Day Hours" << std::endl;
                    for (const auto hours : buf.hourly_msg)
                    {
                        export_file << hours.first << ";" << hours.second << std::endl;
                    }

                    export_file << std::endl << "Messages Per Calender Days" << std::endl;
                    for (const auto days : buf.messages_per_day)
                    {
                        export_file << days.first << ";" << days.second << std::endl;
                    }
                    export_file << std::endl;
                }
            }
            export_file.close();
        }
    }
    
    return 0;
}
