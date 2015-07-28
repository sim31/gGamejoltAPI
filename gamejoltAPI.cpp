#include "gamejoltAPI.h"
#include "core/os/os.h"
#include "core/typedefs.h"
#include "core/error_macros.h"
#include "core/io/json.h"

GamejoltAPI::GamejoltAPI() : m_statusStr("ok"), 
	m_parser(&m_statusStr, &m_status), m_userName(""), 
	m_userToken(""), m_scoreStr("points"), 
	m_status(STATUS_NOT_CONNECTED)
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
	ObjectTypeDB::bind_method("get_status_str", &GamejoltAPI::GetStatusStr);
	ObjectTypeDB::bind_method("get_status", &GamejoltAPI::GetStatus);
	ObjectTypeDB::bind_method("get_http_status", 
			&GamejoltAPI::GetHttpStatus);
	ObjectTypeDB::bind_integer_constant("GamejoltAPI", "STATUS_OK", 0);
	ObjectTypeDB::bind_integer_constant("GamejoltAPI", "STATUS_NOT_CONNECTED",
			1);
	ObjectTypeDB::bind_integer_constant("GamejoltAPI", "STATUS_CANT_CONNECT",
		   	2);
	ObjectTypeDB::bind_integer_constant("GamejoltAPI", 
			"STATUS_CANT_READ_RESPONSE", 3);
	ObjectTypeDB::bind_integer_constant("GamejoltAPI", 
			"STATUS_REQUEST_FAILED", 4);
	ObjectTypeDB::bind_integer_constant("GamejoltAPI", 
			"STATUS_PARSING_ERROR", 5);
	ObjectTypeDB::bind_integer_constant("GamejoltAPI", 
			"STATUS_CANT_SEND_REQUEST", 6);
	ObjectTypeDB::bind_integer_constant("GamejoltAPI", 
			"STATUS_NO_RESPONSE", 7);
	ObjectTypeDB::bind_integer_constant("GamejoltAPI", 
			"STATUS_NO_CREDENTIALS", 8);
}


bool GamejoltAPI::Init(String gameId, String privateKey)
{
	Error err = m_client.connect("www.gamejolt.com", 80);
	if (err != OK)
	{
		m_statusStr = "error connecting: " + itos(err);
		print_line(m_statusStr);
		m_status = STATUS_CANT_CONNECT;
		return false;
	}

	m_statusStr = "connecting";
	while(m_client.get_status()==HTTPClient::STATUS_CONNECTING || 
			m_client.get_status()==HTTPClient::STATUS_RESOLVING)
	{
        m_client.poll();
		print_line("GamejoltAPI::Connecting... ");
		OS::get_singleton()->delay_usec(500);
	}
	if ( m_client.get_status() != HTTPClient::STATUS_CONNECTED )
	{
		m_statusStr = "Failed to connect. HTTPclient status: " +
			itos(m_client.get_status());

		print_line("GamejoltAPI::Init(): " + m_statusStr);
		m_status = STATUS_CANT_CONNECT;
		return false;
	}
	print_line("GamejoltAPI::Connected!");
	m_statusStr = "ok";
	m_status = STATUS_OK;
	m_gameId = gameId;
	m_privateKey = privateKey;
	return true;
}

bool GamejoltAPI::Login(String username, String userToken)
{
	m_status = STATUS_OK;
	if (m_client.get_status() != HTTPClient::STATUS_CONNECTED)
	{
		m_statusStr = "GamejoltAPI::Login(): not connected";
		m_status = STATUS_NOT_CONNECTED; 
		print_line(m_statusStr);
		return false;
	}
	String url = "/api/game/v1/users/auth/?game_id="+
		m_gameId+"&username="+username+"&user_token="+userToken;
	m_statusStr = "requesting authentification";
	print_line(m_statusStr);
	m_statusStr = "ok";
	Dictionary response = MakeRequest(url);
	
	if (m_statusStr == "ok" && response["success"] == "true")
	{
		m_statusStr = "ok";
		m_userName = username;
		m_userToken = userToken;
		m_status = STATUS_OK;
		return true;
	}
	else
	{
		if (m_statusStr != "ok")
			print_line(m_statusStr+"1");
		else //if (response["success"] != "true")
			m_status = STATUS_REQUEST_FAILED;
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
		m_statusStr = "GamejoltAPI::ReadResponse(): couldnt read response";
		m_status = STATUS_CANT_READ_RESPONSE;
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
			*mp_statusStr = "Parsing error";
			*mp_status = STATUS_PARSING_ERROR;
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
	if (m_client.get_status() != HTTPClient::STATUS_CONNECTED)
	{
		m_statusStr = "GamejoltAPI::Request(): not connected";
		m_status = STATUS_NOT_CONNECTED; 
		print_line(m_statusStr);
		return false;
	}
	String urlCopy = "http://gamejolt.com"+url+m_privateKey;
	String md5hash = "&signature="+urlCopy.md5_text();
	url += md5hash;
//	print_line(url);
	Vector<String> headers;
	headers.push_back("User-Agent: Pirulo/1.0 (Godot)");
	headers.push_back("Accept: */*");
	Error result = m_client.request(HTTPClient::METHOD_GET, url, 
		   headers);
	if (result != OK)
	{
		m_status = STATUS_CANT_SEND_REQUEST;
		m_statusStr = "GamejoltAPI::Request(): can't send request";
		return Dictionary();
	}
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
		m_statusStr = "ok";
		m_status = STATUS_OK;
		response = ReadResponse();

		if (m_statusStr != "ok")
		{
			return response;
		}
		String str = response["message"];
		if (str != "")
			m_statusStr = str;
	}
	else
	{
		m_statusStr = "GamejoltAPI::Request(): no response.";
		m_status = STATUS_NO_RESPONSE;
		print_line(m_statusStr);
	}

	return response;

}

bool GamejoltAPI::SendScore(int score, String scboardId,
		String guestName)
{
	m_status = STATUS_OK;
	if (m_client.get_status() != HTTPClient::STATUS_CONNECTED)
	{
		m_statusStr = "GamejoltAPI::SendScore(): not connected";
		m_status = STATUS_NOT_CONNECTED;
		print_line(m_statusStr);
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
			m_statusStr = "GamejoltAPI::SendScore(): cant send score without \
						logging in, or specifying guest name";
			print_line(m_statusStr);
			m_status = STATUS_NO_CREDENTIALS;
			return false;
		}
		url += "&username="+m_userName+"&user_token="+m_userToken;
	}
	else
		url += "&guest="+guestName;
	if (scboardId.length() > 0)
		url += "&table_id="+scboardId;
	Dictionary resp = MakeRequest(url);
	if (m_statusStr == "ok" && resp["success"] == "true")
		return true;
	else
	{
		print_line(m_statusStr);
		m_status = STATUS_REQUEST_FAILED;
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

Dictionary GamejoltAPI::MakeRequest(String& url)
{
	int tries = 0;
	Dictionary resp = Request(url);
	while (tries < 4 && (m_status != STATUS_OK || 
				!(resp["success"] == "true")))
	{
		if (m_status == STATUS_NOT_CONNECTED)
			Init(m_gameId, m_privateKey);
		resp = Request(url);
		tries++;
	}
	return resp;
}
