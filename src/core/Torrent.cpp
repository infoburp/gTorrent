#include "Core.hpp"
#include "Torrent.hpp"
#include "Log.hpp"
#define T_PPM 1000000.f

int intPow(int x, int p)
{
	if (p == 0) return 1;
	if (p == 1) return x;
	int tmp = intPow(x, p / 2);
	if (p % 2 == 0) return tmp * tmp;
	else return x * tmp * tmp;
}

string getTimeString(boost::int64_t time_s)
{
	if ( time_s <= 0 )
		return "???";
	ostringstream time_string;
	int time = time_s, day, hour, min, sec;
	day = time / (3600 * 24);
	hour = time / 3600;
	time = time % 3600;
	min = time / 60;
	time = time % 60;
	sec = time;
	time_string << day << "::" << hour << ":" << min << ":" << sec;
	return time_string.str();
}

string Torrent::getTextFileSize(boost::int64_t fs)
{
	std::ostringstream tfs;
	tfs << fixed << setprecision(3);
	if (fs <= 0)
	{
		tfs << string();
	}
	else if (fs > 0 && fs < 1024)
	{
		tfs << fs << " B";
	}
	else if (fs >= 1024 && fs < intPow(1024,2))
	{
		tfs << fs / 1024 << " KB";
	}
	else if (fs >= intPow(1024,2 && fs < intPow(1024,3))
	{
		tfs << (fs / intPow(1024,2) << " MB";
	}
	else if (fs >= intPow(1024,3))
	{
		tfs << fs / intPow(1024,3) << " GB";
	}
	return tfs.str();
}

string getRateString(boost::int64_t file_rate)
{
	ostringstream file_rate_string;
	file_rate_string << getFileSizeString(file_rate) << "/s";
	return file_rate_string.str();
}

Torrent::Torrent(string path) :
	m_path(path)
{
	setSavePath(""); //TODO add argument to allow user to override the default save path of $HOME/Downloads
	if (gt::Core::isMagnetLink(path))
		m_torrent_params.url = path;
	else
	{
		//libtorrent::add_torrent_params.ti is an intrusive_ptr in 1.0 and a shared_ptr in svn.
		//Using decltype allows us to make it compatible with both versions while still properly using the constructor to avoid a compiler error on boost 1.55 when the = operator is used with a pointer.
		libtorrent::error_code ec;
		decltype(m_torrent_params.ti) tester = decltype(m_torrent_params.ti)(new libtorrent::torrent_info(path, ec));
		if (ec.value() == 0)
		{
			ifstream torrentcheck(path);
			bool isempty = torrentcheck.peek() == ifstream::traits_type::eof();
			torrentcheck.close();
			if (isempty)
			{
				gt::Log::Debug("The torrent file was empty");
				throw - 1;
			}
			m_torrent_params.ti = tester;//If no exception was thrown add the torrent
		}
		else
		{
			gt::Log::Debug(ec.message().c_str());//Call deconstructor?
			throw - 1; //Throw error if construction of libtorrent::torrent_info fails.
		}
	}
}

void Torrent::setSavePath(string savepath)
{
	if (savepath.empty())
		savepath = gt::Core::getDefaultSavePath();
	if (savepath.empty())
		savepath = "./"; //Fall back to ./ if $HOME is not set
	m_torrent_params.save_path = savepath;
}

bool Torrent::pollEvent(gt::Event &event)
{
	if (getTotalProgress() >= 100)
	{
		event.type = gt::Event::DownloadCompleted;
		return true;
	}

	return false;
}

string Torrent::getTextState()
{
	switch (getState())
	{
	case libtorrent::torrent_status::downloading_metadata:
		return "Leeching Metadata";
		break;
	case libtorrent::torrent_status::queued_for_checking:
		return "Queued For Check";
		break;
	case libtorrent::torrent_status::finished:
		return "Finished";
		break;
	case libtorrent::torrent_status::allocating:
		return "Allocating";
		break;
	case libtorrent::torrent_status::checking_resume_data:
		return "Checking Resume";
		break;
	case libtorrent::torrent_status::checking_files:
		return "Checking Files";
		break;
	case libtorrent::torrent_status::seeding:
		return "Seeding";
		break;
	case libtorrent::torrent_status::downloading:
		return "Leeching";
		break;
		//case libtorrent::torrent_status::paused:
		//return "Paused";
		//break;
	}
}

float Torrent::getTotalRatio()
{
	if ( getTotalDownloaded() > 0 )
		return float( getTotalUploaded() ) / float( getTotalDownloaded() );
	else
		return 0.0f;
}

string Torrent::getTextTotalRatio()
{
	ostringstream ttr;
	ttr << fixed << setprecision(3) << getTotalRatio();
	return ttr.str();
}

void Torrent::setPaused(bool isPaused)
{
	m_handle.auto_managed(!isPaused);
	if ( isPaused )
		m_handle.pause();
	else
		m_handle.resume();
}
