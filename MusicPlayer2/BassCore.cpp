#include "stdafx.h"
#include "BassCore.h"
#include "Common.h"
#include "AudioCommon.h"
#include "MusicPlayer2.h"

CBASSMidiLibrary CBassCore::m_bass_midi_lib;
CBassCore::MidiLyricInfo CBassCore::m_midi_lyric;
BASS_MIDI_FONT CBassCore::m_sfont{};

CBassCore::CBassCore()
{
}


CBassCore::~CBassCore()
{
}

void CBassCore::InitCore()
{
    //��ȡ��ǰ����Ƶ����豸
    BASS_DEVICEINFO device_info;
    int rtn;
    int device_index{ 1 };
    theApp.m_output_devices.clear();
    DeviceInfo device{};
    device.index = -1;
    device.name = CCommon::LoadText(IDS_DEFAULT_OUTPUT_DEVICE);
    theApp.m_output_devices.push_back(device);
    while (true)
    {
        device = DeviceInfo{};
        rtn = BASS_GetDeviceInfo(device_index, &device_info);
        if (rtn == 0)
            break;
        device.index = device_index;
        if (device_info.name != nullptr)
            device.name = CCommon::StrToUnicode(string(device_info.name));
        if (device_info.driver != nullptr)
            device.driver = CCommon::StrToUnicode(string(device_info.driver));
        device.flags = device_info.flags;
        theApp.m_output_devices.push_back(device);
        device_index++;
    }

    for (size_t i{}; i < theApp.m_output_devices.size(); i++)
    {
        if (theApp.m_output_devices[i].name == theApp.m_play_setting_data.output_device)
        {
            theApp.m_play_setting_data.device_selected = i;
            break;
        }
    }

    //��ʼ��BASE��Ƶ��
    BASS_Init(
        theApp.m_output_devices[theApp.m_play_setting_data.device_selected].index,		//�����豸
        44100,//���������44100������ֵ��
        BASS_DEVICE_CPSPEAKERS,//�źţ�BASS_DEVICE_CPSPEAKERS ע��ԭ�����£�
        /* Use the Windows control panel setting to detect the number of speakers.*/
        /* Soundcards generally have their own control panel to set the speaker config,*/
        /* so the Windows control panel setting may not be accurate unless it matches that.*/
        /* This flag has no effect on Vista, as the speakers are already accurately detected.*/
        theApp.m_pMainWnd->m_hWnd,//���򴰿�,0���ڿ���̨����
        NULL//���ʶ��,0ʹ��Ĭ��ֵ
    );

    //��֧�ֵ��ļ��б�����ԭ��֧�ֵ��ļ���ʽ
    CAudioCommon::m_surpported_format.clear();
    SupportedFormat format;
    format.description = CCommon::LoadText(IDS_BASIC_AUDIO_FORMAT);
    format.extensions.push_back(L"mp3");
    format.extensions.push_back(L"wma");
    format.extensions.push_back(L"wav");
    format.extensions.push_back(L"flac");
    format.extensions.push_back(L"ogg");
    format.extensions.push_back(L"oga");
    format.extensions.push_back(L"m4a");
    format.extensions.push_back(L"mp4");
    format.extensions.push_back(L"cue");
    format.extensions.push_back(L"mp2");
    format.extensions.push_back(L"mp1");
    format.extensions.push_back(L"aif");
    format.extensions_list = L"*.mp3;*.wma;*.wav;*.flac;*.ogg;*.oga;*.m4a;*.mp4;*.cue;*.mp2;*.mp1;*.aif";
    CAudioCommon::m_surpported_format.push_back(format);
    CAudioCommon::m_all_surpported_extensions = format.extensions;
    //����BASS���
    wstring plugin_dir;
    plugin_dir = theApp.m_local_dir + L"Plugins\\";
    vector<wstring> plugin_files;
    CCommon::GetFiles(plugin_dir + L"*.dll", plugin_files);		//��ȡPluginsĿ¼�����е�dll�ļ����ļ���
    m_plugin_handles.clear();
    for (const auto& plugin_file : plugin_files)
    {
        //���ز��
        HPLUGIN handle = BASS_PluginLoad((plugin_dir + plugin_file).c_str(), 0);
        m_plugin_handles.push_back(handle);
        //��ȡ���֧�ֵ���Ƶ�ļ�����
        const BASS_PLUGININFO* plugin_info = BASS_PluginGetInfo(handle);
        if (plugin_info == nullptr)
            continue;
        format.file_name = plugin_file;
        format.description = CCommon::ASCIIToUnicode(plugin_info->formats->name);	//���֧���ļ����͵�����
        format.extensions_list = CCommon::ASCIIToUnicode(plugin_info->formats->exts);	//���֧���ļ���չ���б�
        //������չ���б���vector
        format.extensions.clear();
        size_t index = 0, last_index = 0;
        while (true)
        {
            index = format.extensions_list.find(L"*.", index + 1);
            wstring ext{ format.extensions_list.substr(last_index + 2, index - last_index - 2) };
            if (!ext.empty() && ext.back() == L';')
                ext.pop_back();
            format.extensions.push_back(ext);
            if (!CCommon::IsItemInVector(CAudioCommon::m_all_surpported_extensions, ext))
                CAudioCommon::m_all_surpported_extensions.push_back(ext);
            if (index == wstring::npos)
                break;
            last_index = index;
        }
        CAudioCommon::m_surpported_format.push_back(format);

        //����MIDI��ɫ�⣬���ڲ���MIDI
        if (format.description == L"MIDI")
        {
            m_bass_midi_lib.Init(plugin_dir + plugin_file);
            m_sfont_name = CCommon::LoadText(_T("<"), IDS_NONE, _T(">"));
            m_sfont.font = 0;
            if (m_bass_midi_lib.IsSuccessed())
            {
                wstring sf2_path = theApp.m_general_setting_data.sf2_path;
                if (!CCommon::FileExist(sf2_path))		//������õ���ɫ��·�������ڣ����.\Plugins\soundfont\Ŀ¼�²�����ɫ���ļ�
                {
                    vector<wstring> sf2s;
                    CCommon::GetFiles(plugin_dir + L"soundfont\\*.sf2", sf2s);
                    if (!sf2s.empty())
                        sf2_path = plugin_dir + L"soundfont\\" + sf2s[0];
                }
                if (CCommon::FileExist(sf2_path))
                {
                    m_sfont.font = m_bass_midi_lib.BASS_MIDI_FontInit(sf2_path.c_str(), BASS_UNICODE);
                    if (m_sfont.font == 0)
                    {
                        CString info;
                        info = CCommon::LoadTextFormat(IDS_SOUND_FONT_LOAD_FAILED, { sf2_path });
                        theApp.WriteErrorLog(info.GetString());
                        m_sfont_name = CCommon::LoadText(_T("<"), IDS_LOAD_FAILED, _T(">"));
                    }
                    else
                    {
                        //��ȡ��ɫ����Ϣ
                        BASS_MIDI_FONTINFO sfount_info;
                        m_bass_midi_lib.BASS_MIDI_FontGetInfo(m_sfont.font, &sfount_info);
                        m_sfont_name = CCommon::StrToUnicode(sfount_info.name);
                    }
                    m_sfont.preset = -1;
                    m_sfont.bank = 0;
                }
            }
        }
    }

}

void CBassCore::UnInitCore()
{
    BASS_Stop();	//ֹͣ���
    BASS_Free();	//�ͷ�Bass��Դ
    if (m_bass_midi_lib.IsSuccessed() && m_sfont.font != 0)
        m_bass_midi_lib.BASS_MIDI_FontFree(m_sfont.font);
    m_bass_midi_lib.UnInit();
    for (const auto& handle : m_plugin_handles)		//�ͷŲ�����
    {
        BASS_PluginFree(handle);
    }
}

unsigned int CBassCore::GetHandle()
{
    return m_musicStream;
}

std::wstring CBassCore::GetAudioType()
{
    return CAudioCommon::GetBASSChannelDescription(m_channel_info.ctype);
}

void CBassCore::MidiLyricSync(HSYNC handle, DWORD channel, DWORD data, void * user)
{
    if (!m_bass_midi_lib.IsSuccessed())
        return;
    m_midi_lyric.midi_no_lyric = false;
    BASS_MIDI_MARK mark;
    m_bass_midi_lib.BASS_MIDI_StreamGetMark(channel, (DWORD)user, data, &mark); // get the lyric/text
    if (mark.text[0] == '@') return; // skip info
    if (mark.text[0] == '\\')
    {
        // clear display
        m_midi_lyric.midi_lyric.clear();
    }
    else if (mark.text[0] == '/')
    {
        m_midi_lyric.midi_lyric += L"\r\n";
        const char* text = mark.text + 1;
        m_midi_lyric.midi_lyric += CCommon::StrToUnicode(text, CodeType::ANSI);
    }
    else
    {
        m_midi_lyric.midi_lyric += CCommon::StrToUnicode(mark.text, CodeType::ANSI);
    }
}

void CBassCore::MidiEndSync(HSYNC handle, DWORD channel, DWORD data, void * user)
{
    m_midi_lyric.midi_lyric.clear();
}

void CBassCore::GetMidiPosition()
{
    if (m_is_midi)
    {
        //��ȡmidi���ֵĽ��Ȳ�ת���ɽ�������������+ (m_midi_info.ppqn / 4)��Ŀ����������ʾ�Ľ��Ĳ�׼ȷ�����⣩
        m_midi_info.midi_position = static_cast<int>((BASS_ChannelGetPosition(m_musicStream, BASS_POS_MIDI_TICK) + (m_midi_info.ppqn / 4)) / m_midi_info.ppqn);
    }

}

int CBassCore::GetChannels()
{
    return m_channel_info.chans;
}

int CBassCore::GetFReq()
{
    return m_channel_info.freq;
}

const wstring& CBassCore::GetSoundFontName()
{
    return m_sfont_name;
}

void CBassCore::Open(const wchar_t * file_path)
{
    m_musicStream = BASS_StreamCreateFile(FALSE, /*(GetCurrentFilePath()).c_str()*/file_path, 0, 0, BASS_SAMPLE_FLOAT);
    BASS_ChannelGetInfo(m_musicStream, &m_channel_info);
    m_is_midi = (CAudioCommon::GetAudioTypeByBassChannel(m_channel_info.ctype) == AudioType::AU_MIDI);
    if (m_bass_midi_lib.IsSuccessed() && m_is_midi && m_sfont.font != 0)
        m_bass_midi_lib.BASS_MIDI_StreamSetFonts(m_musicStream, &m_sfont, 1);

    //����ļ���MIDI���֣����ʱ��ȡMIDI���ֵ���Ϣ
    if (m_is_midi && m_bass_midi_lib.IsSuccessed())
    {
        //��ȡMIDI������Ϣ
        BASS_ChannelGetAttribute(m_musicStream, BASS_ATTRIB_MIDI_PPQN, &m_midi_info.ppqn); // get PPQN value
        m_midi_info.midi_length = static_cast<int>(BASS_ChannelGetLength(m_musicStream, BASS_POS_MIDI_TICK) / m_midi_info.ppqn);
        m_midi_info.tempo = m_bass_midi_lib.BASS_MIDI_StreamGetEvent(m_musicStream, 0, MIDI_EVENT_TEMPO);
        m_midi_info.speed = 60000000 / m_midi_info.tempo;
        //��ȡMIDI������Ƕ���
        BASS_MIDI_MARK mark;
        m_midi_lyric.midi_lyric.clear();
        if (m_bass_midi_lib.BASS_MIDI_StreamGetMark(m_musicStream, BASS_MIDI_MARK_LYRIC, 0, &mark)) // got lyrics
            BASS_ChannelSetSync(m_musicStream, BASS_SYNC_MIDI_MARK, BASS_MIDI_MARK_LYRIC, MidiLyricSync, (void*)BASS_MIDI_MARK_LYRIC);
        else if (m_bass_midi_lib.BASS_MIDI_StreamGetMark(m_musicStream, BASS_MIDI_MARK_TEXT, 20, &mark)) // got text instead (over 20 of them)
            BASS_ChannelSetSync(m_musicStream, BASS_SYNC_MIDI_MARK, BASS_MIDI_MARK_TEXT, MidiLyricSync, (void*)BASS_MIDI_MARK_TEXT);
        BASS_ChannelSetSync(m_musicStream, BASS_SYNC_END, 0, MidiEndSync, 0);
        m_midi_lyric.midi_no_lyric = true;
    }

}

void CBassCore::Close()
{
    BASS_StreamFree(m_musicStream);
}

void CBassCore::Play()
{
    BASS_ChannelPlay(m_musicStream, FALSE);
}

void CBassCore::Pause()
{
    BASS_ChannelPause(m_musicStream);
}

void CBassCore::Stop()
{
    BASS_ChannelStop(m_musicStream);
}

void CBassCore::SetVolume(int vol)
{
    float volume = static_cast<float>(vol) / 100.0f;
    volume = volume * theApp.m_nc_setting_data.volume_map / 100;
    BASS_ChannelSetAttribute(m_musicStream, BASS_ATTRIB_VOL, volume);
}

int CBassCore::GetCurPosition()
{
    QWORD pos_bytes;
    pos_bytes = BASS_ChannelGetPosition(m_musicStream, BASS_POS_BYTE);
    double pos_sec;
    pos_sec = BASS_ChannelBytes2Seconds(m_musicStream, pos_bytes);
    int current_position = static_cast<int>(pos_sec * 1000);
    if (current_position == -1000) current_position = 0;

    GetMidiPosition();
    return current_position;
}

int CBassCore::GetSongLength()
{
    QWORD lenght_bytes;
    lenght_bytes = BASS_ChannelGetLength(m_musicStream, BASS_POS_BYTE);
    double length_sec;
    length_sec = BASS_ChannelBytes2Seconds(m_musicStream, lenght_bytes);
    int song_length = static_cast<int>(length_sec * 1000);
    if (song_length == -1000) song_length = 0;
    return song_length;
}

void CBassCore::SetCurPosition(int position)
{
    double pos_sec = static_cast<double>(position) / 1000.0;
    QWORD pos_bytes;
    pos_bytes = BASS_ChannelSeconds2Bytes(m_musicStream, pos_sec);
    BASS_ChannelSetPosition(m_musicStream, pos_bytes, BASS_POS_BYTE);
    m_midi_lyric.midi_lyric.clear();
    GetMidiPosition();
}

bool CBassCore::IsMidi()
{
    return m_is_midi;
}

bool CBassCore::IsMidiConnotPlay()
{
    return (m_is_midi && m_sfont.font == 0);
}

std::wstring CBassCore::GetMidiInnerLyric()
{
    return m_midi_lyric.midi_lyric;
}

MidiInfo CBassCore::GetMidiInfo()
{
    return m_midi_info;
}

bool CBassCore::MidiNoLyric()
{
    return m_midi_lyric.midi_no_lyric;
}