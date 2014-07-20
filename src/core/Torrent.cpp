#include <core/Core.hpp>
#include "Torrent.hpp"
#include "Log.hpp"
#define T_PPM 1000000.f

int intPow(int x, int p)
{
	if (p == 0) return 1;
	if (p == 1) return x;
	int tmp = intPow(x, p/2);
	if (p%2 == 0) return tmp * tmp;
	else return x * tmp * tmp;
}

string getTimeString(boost::int64_t time_s)
{
	if ( time_s <= 0 )
		return "???";

	boost::int64_t time_m = (time_s - (fmod (time_s, time_s / 60))) / 60;
	boost::int64_t time_h = (time_m - (fmod (time_m, time_m / 60))) / 60;
	boost::int64_t time_d = (time_h - (fmod (time_h, time_h / 24))) / 24;

	ostringstream time_string;

	if(time_s <= 0)
	{
		return string();
	}
	if(time_s < 60)
	{
		if(time_s == 1)
		{
			time_string << time_s << " Second";
		}
		else
		{
			time_string << time_s << " Seconds";
		}
	}
	if(time_s >= 60 && time_s < (60 * 60))
	{
		if(time_m == 1)
		{
			time_string << time_m << " Minute, ";
		}
		else
		{
			time_string << time_m << " Minutes, ";
		}
	}
	if(time_m >= 60 && time_m < (60 * 60))
	{
		if(time_h == 1)
		{
			time_string << time_h << " Hour, ";
		}
		else
		{
			time_string << time_h << " Hours, ";
		}
	}
	if(time_h >= 24)
	{
		if(time_h == 1)
		{
			time_string << time_d << " Day, ";
		}
		else
		{
			time_string << time_d << " Days, ";
		}
	}
	return time_string.str();
}

string getFileMetaDataString(boost::int64_t file_size, bool rate)
{
	if (file_size <= 0)
	{
		return string();
	}

	ostringstream file_meta_string;
	array<string, 4> items   = {" GB"," MB"," KB"," B"};
	array<int, 4> item_sizes = {intPow(1024,3),intPow(1024,2),1024,1};

	for_each(items.begin(), items.end(), [&](string items, int item_sizes)
	{
		file_meta_string << fixed << setprecision(3) << (file_size / item_sizes);
		file_meta_string << items;

		if (rate)
		{
			file_meta_string << "/s";
		}
	});

	return file_meta_string.str();
}

string getRateString(boost::int64_t file_rate)
{
	return getFileMetaDataString(file_rate,1);
}

string getFileSizeString(boost::int64_t file_size)
{
	return getFileMetaDataString(file_size,0);
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

libtorrent::add_torrent_params Torrent::getTorrentParams()
{
	return m_torrent_params;
}

libtorrent::torrent_handle &Torrent::getHandle()
{
	return m_handle;
}

string Torrent::getPath()
{
	return m_path;
}

boost::int64_t Torrent::getAge()
{
	return m_handle.status().active_time;
}

string Torrent::getTextAge()
{
	return getTimeString(getAge());
}

boost::int64_t Torrent::getWanted()
{
	return m_handle.status().total_wanted;
}

boost::int64_t Torrent::getEta()
{
	return ( getDownloadRate() <= 0 ) ? -1 : ( getWanted() / getDownloadRate() );
}

string Torrent::getTextEta()
{
	return getTimeString( getEta() );
}

float Torrent::getTotalProgress()
{
	libtorrent::torrent_status s = m_handle.status();

	return ((float) s.progress_ppm / (float) T_PPM) * 100;
}

unsigned int Torrent::getPPMProgress()
{
	libtorrent::torrent_status s = m_handle.status();

	return s.progress_ppm;
}

unsigned int Torrent::getTotalSeeders()
{
	return m_handle.status().num_seeds;
}

unsigned int Torrent::getTotalPeers()
{
	return m_handle.status().num_peers;
}

unsigned int Torrent::getTotalLeechers()
{
	return m_handle.status().num_peers - m_handle.status().num_seeds;
}

libtorrent::torrent_status::state_t Torrent::getState()
{
	return m_handle.status().state;
}

string Torrent::getTextState()
{
	switch (getState())
	{
	case libtorrent::torrent_status::checking_files:
		return "Checking";
		break;
	case libtorrent::torrent_status::seeding:
		return "Seeding";
		break;
	case libtorrent::torrent_status::downloading:
	default:
		ostringstream o;
		int precision = 1;
		if (m_torrent_params.ti != NULL) //m_torrent_params.ti is not initial initialized for magnet links
			//if (m_torrent_params.ti->total_size() < intPow(1024,3)
				//precision = 0;//Set 0 decimal places if file is less than 1 gig.
		o << fixed << setprecision(precision) << getTotalProgress() << " %";
		return o.str();
		break;
	}
}

string Torrent::getCurrentTrackerURL()
{
	return m_handle.status().current_tracker;
}

unsigned int Torrent::getUploadRate()
{
	return m_handle.status().upload_rate;
}

string Torrent::getTextUploadRate()
{
	return getRateString(getUploadRate());
}

unsigned int Torrent::getDownloadRate()
{
	return m_handle.status().download_rate;
}

string Torrent::getTextDownloadRate()
{
	return getRateString(getDownloadRate());
}

boost::int64_t Torrent::getTotalUploaded()
{
	return m_handle.status().total_upload;
}

string Torrent::getTextTotalUploaded()
{
	return getFileSizeString(getTotalUploaded());
}


boost::int64_t Torrent::getTotalDownloaded()
{
	return m_handle.status().total_download;
}

string Torrent::getTextTotalDownloaded()
{
	return getFileSizeString(getTotalDownloaded());
}

boost::int64_t Torrent::getSize()
{
	return m_handle.status().total_wanted;
}

string Torrent::getTextSize()
{
	return getFileSizeString(getSize());
}

boost::int64_t Torrent::getRemaining()
{
	return m_handle.status().total_wanted - m_handle.status().total_wanted_done;
}

string Torrent::getTextRemaining()
{
	return getFileSizeString(getRemaining());
}

boost::int64_t Torrent::getTorrentSize()
{
	return m_handle.status().total_wanted;
}

boost::int64_t Torrent::getTimeRemaining()
{
	if(getDownloadRate() > 0)
		return getTorrentSize() / getDownloadRate();
	else
		return 0;
}

string Torrent::getTextTimeRemaining()
{
	return getTimeString(getTimeRemaining());
}

float Torrent::getTotalRatio()
{
	if (getTotalDownloaded() == 0)
	{
		return 0.0f;
	}
	else
	{
		float totalRatio = float(getTotalUploaded()) / float(getTotalDownloaded());
		return totalRatio;
	}
}

string Torrent::getTextTotalRatio()
{
	stringstream ttr (stringstream::in | stringstream::out);

	float ratio = getTotalRatio();

	ttr << fixed << setprecision(3) << ratio;

	return ttr.str();
}

void Torrent::setHandle(libtorrent::torrent_handle &h)
{
	m_handle = h;
}

void Torrent::setPaused(bool isPaused)
{
	m_handle.auto_managed(!isPaused);
	isPaused ?
	m_handle.pause()
	:
	m_handle.resume();
}

bool Torrent::isPaused()
{
	return m_handle.status().paused;
}

void Torrent::resume()
{
	setPaused(false);
}

void Torrent::pause()
{
	setPaused(true);
}
