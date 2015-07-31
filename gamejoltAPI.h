#ifndef GAMEJOLTAPI_H
#define GAMEJOLTAPI_H

#include "reference.h"
#include "core/ustring.h"
#include "core/io/http_client.h"

class GamejoltAPI : public Reference
{
public:
	GamejoltAPI();
	~GamejoltAPI();
	enum Status
	{
		STATUS_OK,
		STATUS_NOT_CONNECTED,
		STATUS_CANT_CONNECT,
		STATUS_CANT_READ_RESPONSE,
		STATUS_REQUEST_FAILED, //means response came back negative
		STATUS_PARSING_ERROR,
		STATUS_CANT_SEND_REQUEST,
		STATUS_NO_RESPONSE,
		STATUS_NO_CREDENTIALS,
	};
	bool Init(String gameId, String privateKey);
	bool Login(String username, String userToken);
	//if scboard blank, default main scoreboard is used
	//if guestName blank, username is used (from Login())
	bool SendScore(int score, String scboardId = "", 
			String guestName = ""); 
	//if scores argument is default value, then scoretable is requested from server
	//returned -1 means error
	int GetUserBest(String scboard = "", Dictionary scores = Dictionary());
	int GetGuestBest(String guestName, String scboard, 
			Dictionary scores = Dictionary());
	//returns score in scoretable in a multidimensional dictionary
	//where first index shows index of score, second returned key
	//ex: dict[0]["score"] - returns score string of first player
	//parameter names can be found here: http://gamejolt.com/api/doc/game/scores/fetch
	Dictionary GetScores(String table_id = "", int limit = 10);
	void Update();
	inline String GetStatusStr()
	{
		return m_statusStr;
	}
	inline Status GetStatus()
	{
		return m_status;
	}
	inline HTTPClient::Status GetHttpStatus()
	{
		return m_client.get_status();
	}
	void SetScoreStr(const String& str) //default (ex): "100 points"
	{
		m_scoreStr = str;
	}
	
private:
	OBJ_TYPE(GamejoltAPI, Reference);

	HTTPClient m_client;
	//game info
	String m_gameId;
	String m_privateKey;
	//player info
	String m_userName;
	String m_userToken;
	String m_statusStr;
	String m_scoreStr;
	Status m_status;

	Dictionary Request(String& url);
	Dictionary MakeRequest(String& url);
	Dictionary ReadResponse();
	String ChunkToStr(const ByteArray& bytes);
	//translates from keypair of gamejolt api to dictionary
	class KeyPairParser
	{
	private:
		String m_currStr;
		int ind;
		int dict_ind;
		String* mp_statusStr;
		Status* mp_status;

		String ReadKey(); //reads next key, increments ind
		String ReadValue(); //reads next value, increments ind
	public:
		KeyPairParser(String* statusStr, Status* status) : 
			ind(0), dict_ind(0), mp_statusStr(statusStr), mp_status(status) {}
		Dictionary Parse(const String& str);
	} m_parser;

	String FixStr(const String& str);
	bool IsConnected(String where);
	int GetBest(const Dictionary& scores, const String& name);
protected:
	static void _bind_methods();	
};

#endif
