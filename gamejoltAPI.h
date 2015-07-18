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
	bool Init(String gameId, String privateKey);
	bool Login(String username, String userToken);
	//if scboard blank, default main scoreboard is used
	//if guestName blank, username is used (from Login)
	bool SendScore(int score, String scboardId = "", 
			String guestName = ""); 
	int GetUserBest();
	int GetGuestBest(String guestName);
	void Update();
	inline String GetStatus()
	{
		return m_status;
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
	String m_status;
	String m_scoreStr;

	Dictionary ReadResponse();
	String ChunkToStr(const ByteArray& bytes);
	//translates from keypair of gamejolt api to dictionary
	class KeyPairParser
	{
	private:
		String m_currStr;
		int ind;
		String* mp_status;

		String ReadKey(); //reads next key, increments ind
		String ReadValue(); //reads next value, increments ind
	public:
		KeyPairParser(String* status) : ind(0), mp_status(status) {}
		Dictionary Parse(const String& str);
	} m_parser;

	Dictionary Request(String& url);
	String FixStr(const String& str);
protected:
	static void _bind_methods();	
};

#endif
