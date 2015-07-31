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
	ObjectTypeDB::bind_method("get_scores", &GamejoltAPI::GetScores,
			"", 10);
	ObjectTypeDB::bind_method("get_user_best", &GamejoltAPI::GetUserBest,
			"", Dictionary());
	ObjectTypeDB::bind_method("get_guest_best", &GamejoltAPI::GetGuestBest,
			"", "", Dictionary());
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
	if (!IsConnected("GamejoltAPI::Login()"))
		return false;
	String url = "/api/game/v1/users/auth/?game_id="+
		m_gameId+"&username="+username+"&user_token="+userToken;
	m_statusStr = "requesting authentification";
	print_line(m_statusStr);
	m_statusStr = "ok";
	Dictionary response = MakeRequest(url);
	Dictionary answ = response[0];	
	if (m_statusStr == "ok" && answ["success"] == "true")
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
	dict[0] = Dictionary();
	m_currStr = str;
	ind = 0;
	dict_ind = 0;
	while (ind < str.length()-1)
	{
		String key = ReadKey();
		String value = ReadValue();
		if (key == "" || value == "-1")
		{
			*mp_statusStr = "Parsing error";
			*mp_status = STATUS_PARSING_ERROR;
			return dict;
		}
		Dictionary d = dict[dict_ind];
		if (!d.has(key))
			d[key] = value;
		else
		{
			dict[++dict_ind] = Dictionary();
			d = dict[dict_ind];
			d[key] = value;
		}
		dict[dict_ind] = d;
	}	
	return dict;
}

String GamejoltAPI::KeyPairParser::ReadKey()
{	
	int newInd = m_currStr.find(":", ind);
	if (newInd < 0)
		return "";
	String r = m_currStr.substr(ind, newInd-ind);
//	print_line(r);
	ind = newInd;
	return r;
}

String GamejoltAPI::KeyPairParser::ReadValue()
{
	int newIndb = m_currStr.find("\"", ind);
	if (newIndb <= 0)
	{
		print_line("KayPairParser::ReadValue()::err1");
		return "-1";
	}
	int newInde = m_currStr.find("\"", newIndb+1);
	if (newInde <= 0)
	{
		print_line("KayPairParser::ReadValue()::err2");
		return "-1";
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
	if (!IsConnected("GamejoltAPI::SendScore()"))
		return false;

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
	Dictionary answ = resp[0];
	if (m_statusStr == "ok" && answ["success"] == "true")
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
	Dictionary answ = resp[0];
	while (tries < 4 && (m_status != STATUS_OK || 
				!(answ["success"] == "true")))
	{
		if (m_status == STATUS_NOT_CONNECTED)
			Init(m_gameId, m_privateKey);
		resp = Request(url);
		answ = resp[0];
		tries++;
	}
	return resp;
}

Dictionary GamejoltAPI::GetScores(String table_id, int limit)
{
	m_status = STATUS_OK;
	if (!IsConnected("GamejoltAPI::GetScores()"))
		return false;
	String url = "/api/game/v1/scores/?game_id="+m_gameId+"&limit="+itos(limit);
	if (table_id.length() > 0)
		url += "&table_id="+table_id;
	Dictionary resp = MakeRequest(url);
	Dictionary answ = resp[0];
	if (answ["success"] == "true")
	{
		answ.erase("success");
		resp[0] = answ;
		return resp;
	}
	else
	{
		print_line(m_statusStr);
		m_status = STATUS_REQUEST_FAILED;
		return resp;
	}
	return resp;
}

int GamejoltAPI::GetUserBest(String scboard, Dictionary scores)
{
	if (m_userName == "" || m_userToken == "")
	{
		m_status = STATUS_NO_CREDENTIALS;	
		m_statusStr = "Requested user best without credentials";
		return -1;
	}
	if (scores.has(0))
		return GetBest(scores, m_userName);
	scores = GetScores(scboard, 100);
	if (m_status != STATUS_OK)
		return -1;
	return GetBest(scores, m_userName);
}

int GamejoltAPI::GetGuestBest(String guestName, String scboard, Dictionary scores)
{
	if (guestName == "")
		return -1;
	if (scores.has(0))
		return GetBest(scores, guestName);
	scores = GetScores(scboard, 100);
	if (m_status != STATUS_OK)
		return -1;
	return GetBest(scores, guestName);
}

int GamejoltAPI::GetBest(const Dictionary& scores, const String& name)
{
	int best = 0;
	int ind = 0;
	while (scores.has(ind))
	{
		Dictionary dict = scores[ind];
		int score = dict["sort"];
		if ((dict["user"] == name || dict["guest"] == name) && 
				best < score)
		{
			best = score;
		}
		ind++;
	}
	return best;
}

bool GamejoltAPI::IsConnected(String where)
{
	if (m_client.get_status() != HTTPClient::STATUS_CONNECTED)
	{
		m_statusStr = where+ ": not connected";
		m_status = STATUS_NOT_CONNECTED;
		print_line(m_statusStr);
		return false;
	}
	else
		return true;
}


