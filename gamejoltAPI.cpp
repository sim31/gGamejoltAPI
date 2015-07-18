#include "gamejoltAPI.h"
#include <iostream>
#include "core/os/os.h"
#include "core/typedefs.h"
#include "core/error_macros.h"
#include "core/io/json.h"

GamejoltAPI::GamejoltAPI() : m_status("ok"), m_parser(&m_status),
	m_userName(""), m_userToken(""), m_scoreStr("points")
{
}

GamejoltAPI::~GamejoltAPI()
{
	m_client.close();
}

void GamejoltAPI::_bind_methods()
{
	ObjectTypeDB::bind_method("update", &GamejoltAPI::Update);
	ObjectTypeDB::bind_method("login", &GamejoltAPI::Login);
	ObjectTypeDB::bind_method("init", &GamejoltAPI::Init);
	ObjectTypeDB::bind_method("send_score", &GamejoltAPI::SendScore, 
			0, "", "");
	ObjectTypeDB::bind_method("get_status", &GamejoltAPI::GetStatus);
}

bool GamejoltAPI::Init(String gameId, String privateKey)
{
	Error err = m_client.connect("www.gamejolt.com", 80);
	if (err != OK)
	{
		m_status = "error connecting: " + itos(err);
		print_line(m_status);
		return false;
	}

	m_status = "connecting";
	while(m_client.get_status()==HTTPClient::STATUS_CONNECTING || 
			m_client.get_status()==HTTPClient::STATUS_RESOLVING)
	{
        m_client.poll();
		print_line("GamejoltAPI::Connecting... ");
		OS::get_singleton()->delay_usec(500);
	}
	if ( m_client.get_status() != HTTPClient::STATUS_CONNECTED )
	{
		m_status = "Failed to connect. HTTPclient status: " +
			itos(m_client.get_status());

		print_line("GamejoltAPI::Init(): " + m_status);
		return false;
	}
	print_line("GamejoltAPI::Connected!");
	m_status = "ok";
	m_gameId = gameId;
	m_privateKey = privateKey;
	return true;
}

bool GamejoltAPI::Login(String username, String userToken)
{
	if (m_client.get_status() != HTTPClient::STATUS_CONNECTED)
	{
		m_status = "GamejoltAPI::Login(): not connected";
		print_line(m_status);
		return false;
	}
	String url = "/api/game/v1/users/auth/?game_id="+
		m_gameId+"&username="+username+"&user_token="+userToken;
	m_status = "requesting authentification";
	print_line(m_status);
	m_status = "ok";
	Dictionary response = Request(url);
	
	if (m_status == "ok" && response["success"] == "true")
	{
		m_status = "ok";
		m_userName = username;
		m_userToken = userToken;
		return true;
	}
	else
	{
		if (m_status != "ok")
			print_line(m_status+"1");
		return false;
	}
}

Dictionary GamejoltAPI::ReadResponse()
{
	String str = "";
	while (m_client.get_status() == HTTPClient::STATUS_BODY)
	{
		m_client.poll();
		ByteArray chunk = m_client.read_response_body_chunk();
		if (chunk.size() == 0)
			OS::get_singleton()->delay_usec(1000);
		else
			str += ChunkToStr(chunk);
	}
	if (str.length() == 0)
	{
		m_status = "GamejoltAPI::ReadResponse(): couldnt read response";
		return Dictionary();
	}
	print_line("unparsed: "+ str);
	Dictionary dict = m_parser.Parse(str);
	return dict;
}

String GamejoltAPI::ChunkToStr(const ByteArray& bytes)
{
	String r = "";
	for (int i = 0; i < bytes.size(); i++)
		r += (char)bytes[i];
	return r;
}

Dictionary GamejoltAPI::KeyPairParser::Parse(const String& str)
{
	Dictionary dict;
	m_currStr = str;
	ind = 0;
	while (ind < str.length()-1)
	{
		String key = ReadKey();
		String value = ReadValue();
		if (key == "" || value == "")
		{
			*mp_status = "Parsing error";
			return dict;
		}
		dict[key] = value;
	}	
	return dict;
}

String GamejoltAPI::KeyPairParser::ReadKey()
{	
	int newInd = m_currStr.find(":", ind);
	if (newInd < 0)
		return "";
	String r = m_currStr.substr(ind, newInd-ind);
	//print_line(r);
	ind = newInd;
	return r;
}

String GamejoltAPI::KeyPairParser::ReadValue()
{
	int newIndb = m_currStr.find("\"", ind);
	if (newIndb < 0)
	{
		print_line("KayPairParser::ReadValue()::err1");
		return "";
	}
	int newInde = m_currStr.find("\"", newIndb+1);
	if (newInde < 0)
	{
		print_line("KayPairParser::ReadValue()::err2");
		return "";
	}
	String r = m_currStr.substr(newIndb+1, newInde-newIndb-1);
	//print_line(r);
	ind = m_currStr.find("\n", newInde)+1;
	return r;
}

void GamejoltAPI::Update()
{
	//TODO: check if ok
	m_client.poll();
}

Dictionary GamejoltAPI::Request(String& url)
{
	String urlCopy = "http://gamejolt.com"+url+m_privateKey;
	String md5hash = "&signature="+urlCopy.md5_text();
	url += md5hash;
	print_line(url);
	Vector<String> headers;
	headers.push_back("User-Agent: Pirulo/1.0 (Godot)");
	headers.push_back("Accept: */*");
	Error result = m_client.request(HTTPClient::METHOD_GET, url, 
		   headers);
	while (m_client.get_status() == HTTPClient::STATUS_REQUESTING)
	{
		m_client.poll();
		print_line("GamejoltAPI::Requesting..");
		OS::get_singleton()->delay_usec(10000);
	}
	print_line("GamejoltAPI::Request sent!");

	Dictionary response;
	if (m_client.has_response())
	{
		m_status = "ok";
		response = ReadResponse();

		if (m_status != "ok")
		{
			return response;
		}
		String str = response["message"];
		if (str != "")
			m_status = str;
	}
	else
	{
		m_status = "GamejoltAPI::Request(): no response.";
		print_line(m_status);
	}

	return response;

}

bool GamejoltAPI::SendScore(int score, String scboardId,
		String guestName)
{
	if (m_client.get_status() != HTTPClient::STATUS_CONNECTED)
	{
		m_status = "GamejoltAPI::SendScore(): not connected";
		print_line(m_status);
		return false;
	}

	String str = itos(score)+ " " + m_scoreStr;

	str = FixStr(str);
	String url = "/api/game/v1/scores/add/?game_id="+m_gameId+
		"&score="+str+"&sort="+itos(score);
	if (guestName == "")
	{
		if (m_userName.length() == 0 || m_userToken.length() == 0)
		{
			m_status = "GamejoltAPI::SendScore(): cant send score without \
						logging in, or specifying guest name";
			print_line(m_status);
			return false;
		}
		url += "&username="+m_userName+"&user_token="+m_userToken;
	}
	else
		url += "&guest="+guestName;
	if (scboardId.length() > 0)
		url += "&table_id="+scboardId;
	Dictionary resp = Request(url);
	if (m_status == "ok" && resp["success"] == "true")
		return true;
	else
	{
		print_line(m_status);
		return false;
	}
}

String GamejoltAPI::FixStr(const String& str)
{
	String str1 = str;
	for (int i = 0; i < str.length(); i++)
	{
		if (str1[i] == ' ')
			str1[i] = '+';
	}
	return str1;
}
