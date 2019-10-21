// Checkpoint.cpp : Defines the entry point for the DLL application.
//   by mrapple
//   https://forums.bzflag.org/viewtopic.php?f=79&t=16273

#include "bzfsAPI.h"
#include "plugin_utils.h"
#include <string>
#include <vector>
#include <map>
#include <math.h>

// Define the CheckpointHandler
class CheckpointHandler : public bz_Plugin, public bz_CustomSlashCommandHandler, public bz_CustomMapObjectHandler
{
public:
	virtual const char* Name (){return "Checkpoint";}
	virtual void Init (const char* config);
	virtual void Event(bz_EventData *eventData);
	virtual void Cleanup ();
	virtual bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params);
	virtual bool MapObject( bz_ApiString object, bz_CustomMapObjectInfo *data );
};

BZ_PLUGIN(CheckpointHandler);

// Load the plugin data
void CheckpointHandler::Init(const char* /*commandLine*/)
{
	Register(bz_eCaptureEvent);
	Register(bz_ePlayerPartEvent);
	Register(bz_eGetPlayerSpawnPosEvent);
	Register(bz_eShotFiredEvent);
	Register(bz_ePlayerUpdateEvent);
	bz_registerCustomMapObject("CHECKPOINT",this);
	bz_registerCustomSlashCommand ("spawnreset",this);
	bz_registerCustomSlashCommand ("spawnresetall",this);
	bz_debugMessage(4,"Checkpoint plugin loaded");
}

// Checking if a user is admin for later in the plugin
bool isAdmin(int playerID)
{
	bz_BasePlayerRecord* player = bz_getPlayerByIndex(playerID);
	bool admin = player->hasPerm("checkpoint");
	bz_freePlayerRecord(player);
	return admin;
}

// Getting players ID by callsign for later in the plugin
int getPlayerByCallsign(bz_ApiString callsign)
{
	bz_APIIntList* players = bz_newIntList();
	bz_getPlayerIndexList(players);
	int id = -1;

	for(unsigned int i = 0; i < players->size(); i++)
	{
		bz_BasePlayerRecord* player = bz_getPlayerByIndex(players->get(i));
		bool found = false;

		if(player->callsign == callsign)
		{
			id = (int)i;
			found = true;
		}

		bz_freePlayerRecord(player);

		if(found)
			break;
	}

	bz_deleteIntList(players);

	return id;
}

// Define playerID
typedef int PlayerID;
// This is where the spawnpoints are kept
class SpawnPoint
{
public:
	SpawnPoint() : x(0), y(0), z(0), theta(0) { }
	SpawnPoint(float _x, float _y, float _z, float _theta) : x(_x), y(_y), z(_z), theta(_theta) { }
	float x, y, z, theta;
};

// Now we are defining the array
std::map<PlayerID, SpawnPoint> spawnPoints;

// This is where the checkpoint data is kept
class CheckPoint
{
public:
	// Checkpoint format
	CheckPoint()
	{
		sizex = sizey = sizez = posx = posy = posz = 0;
	}
	// Define the variables
	float sizex,sizey,sizez,posx,posy,posz;

	std::string message;
	// Check to see if a point is inside a zone
	bool pointIn ( float pos[3] )
	{
		if ( pos[0] > (posx +  sizex) || pos[0] < (posx -  sizex))
			return false;

		if ( pos[1] > (posy +  sizey) || pos[1] < (posy -  sizey))
			return false;

		if ( pos[2] > (posz +  sizez) || pos[2] < (posz -  sizez))
			return false;

		return true;
	}

};

// Make the vector list
std::vector <CheckPoint> zoneList;

// Now we are going to check the map for CHECKPOINT objects
bool CheckpointHandler::MapObject( bz_ApiString object, bz_CustomMapObjectInfo *data )
{
	//Is this the correct object?
	if (object != "CHECKPOINT" || !data)
		return false;

	CheckPoint newZone;

	// Parse all the chunks
	for ( unsigned int i = 0; i < data->data.size(); i++ )
	{
		// Get a single line
		std::string line = data->data.get(i).c_str();
		// Parse the line
		bz_APIStringList *nubs = bz_newStringList();
		nubs->tokenize(line.c_str()," ",0,true);
		// If we have something on that line
		if ( nubs->size() > 0)
		{
			std::string key = bz_toupper(nubs->get(0).c_str());

			// Read the position. Requires 3 values.
			if ( key == "POSITION" && nubs->size() > 3)
			{
				newZone.posx = (float)atof(nubs->get(1).c_str());
				newZone.posy = (float)atof(nubs->get(2).c_str());
				newZone.posz = (float)atof(nubs->get(3).c_str());
			}
			// Read the size. Requires 3 value.
			else if (key == "SIZE" && nubs->size() > 3)
			{
				newZone.sizex = (float)atof(nubs->get(1).c_str());
				newZone.sizey = (float)atof(nubs->get(2).c_str());
				newZone.sizez = (float)atof(nubs->get(3).c_str());
			}
			// Read the message
			else if ( key == "MESSAGE" && nubs->size() > 1 )
			{
				newZone.message = nubs->get(1).c_str();
			}
		}
		bz_deleteStringList(nubs);
	}
	// Save every zone
	zoneList.push_back(newZone);
	return true;
}

std::map<int,int>	playeIDToZoneMap;

// Actions to run when events happen
void CheckpointHandler::Event(bz_EventData *eventData)
{
	// Define pos and playerID
	float pos[3] = {0};
	int playerID = NULL;

	switch (eventData->eventType)
	{
		case bz_eCaptureEvent: // If a team's flag is captured
		{
			// Clear the spawnpoints
			spawnPoints.clear();
		}
			break;

		case bz_ePlayerPartEvent: // If A player leaves the game
		{
			// Get part game data
			bz_PlayerJoinPartEventData_V1* partspawndata = (bz_PlayerJoinPartEventData_V1*)eventData;
			std::map<PlayerID, SpawnPoint>::iterator sPoint = spawnPoints.find(partspawndata->playerID);
			// Erase their data
			if(sPoint != spawnPoints.end()){
				spawnPoints.erase(sPoint);
			}
		}
			break;

		case bz_eGetPlayerSpawnPosEvent: // If A spawn location is selected
		{
			// Get spawn location data
			bz_GetPlayerSpawnPosEventData_V1* spawndata = (bz_GetPlayerSpawnPosEventData_V1*)eventData;
			std::map<PlayerID, SpawnPoint>::iterator sPoint = spawnPoints.find(spawndata->playerID);
			// If user spawns on same position as saved position, spawn them
			if(sPoint == spawnPoints.end()){
				break;
			}
			// Set the players spawn position to their last saved position
			const SpawnPoint &pos = sPoint->second;
			spawndata->handled = true;
			spawndata->pos[0] = pos.x;
			spawndata->pos[1] = pos.y;
			spawndata->pos[2] = pos.z;
			spawndata->rot = pos.theta;
		}
			break;

		case bz_eShotFiredEvent: // If A shot is fired
		{
			// Get users position
			pos[0] = ((bz_ShotFiredEventData_V1*)eventData)->pos[0];
			pos[1] = ((bz_ShotFiredEventData_V1*)eventData)->pos[1];
			pos[2] = ((bz_ShotFiredEventData_V1*)eventData)->pos[2];
			// Get their playerID
			playerID = ((bz_ShotFiredEventData_V1*)eventData)->playerID;
		}
			break;

		case bz_ePlayerUpdateEvent: // If A player sends a position update
		{
			// Get playerID
			playerID = ((bz_PlayerUpdateEventData_V1*)eventData)->playerID;
			// Get player data
			bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
			// If the player is spawned, send the data
			// We dont want users sending data while they are dead
			if ( player->spawned ){
				pos[0] = player->lastKnownState.pos[0];
				pos[1] = player->lastKnownState.pos[1];
				pos[2] = player->lastKnownState.pos[2];
			}
			// Clear the record
			bz_freePlayerRecord(player);
		}
			break;

		default:
			break;

	}

	std::vector<CheckPoint*> validZones;

	// Check to see if the zone is valid
	for ( unsigned int i = 0; i < zoneList.size(); i++ )
	{
		validZones.push_back(&zoneList[i]);
	}
	// insideOne is default false and only turns true when we are inside a zone
	bool insideOne = false;
	// currentZone is the zone we are in, default null
	int currentZone = NULL;
	// lastZone is the last zone we were in, default null
	int lastZone = NULL;
	// Now we are setting the lastZone to the current zone we are in
	lastZone = playeIDToZoneMap[playerID];
	// Check each zone to see if we are in one
	for ( unsigned int i = 0; i < validZones.size(); i++ )
	{
		if ( validZones[i]->pointIn(pos) )
		{
			// If we are inside a zone, insideOne is true and update our currentZone
			insideOne = true;
			playeIDToZoneMap[playerID] = i;
			currentZone = playeIDToZoneMap[playerID];
		}
	}

	// If we are in one.
	if (insideOne && validZones.size() > 0)
	{
		// Get player data
		bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
		// Update spawn coordinates inside the zone
		spawnPoints[playerID] =
		SpawnPoint(player->lastKnownState.pos[0], player->lastKnownState.pos[1], player->lastKnownState.pos[2], player->lastKnownState.rotation);
		// If we have moved to a new zone, send a new message
		if(lastZone != currentZone){
			if(player) {
				bz_freePlayerRecord(player);
			}
			if (currentZone && zoneList[currentZone].message.size()){
				// If we have a zone and theres a message
				bz_sendTextMessage(BZ_SERVER,playerID,zoneList[currentZone].message.c_str());
			} else {
				// Otherwise send the default message
				bz_sendTextMessage(BZ_SERVER, playerID, "Saved new spawn position");
			}
		}
	}
}
// Next we are going to handle commands
bool CheckpointHandler::SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList * /*_params*/)
{
	// If user types /spawnreset
	if(command == "spawnreset")
	{
		// Get the reset of the parameters and tokenize them
		bz_APIStringList params;
		params.tokenize(message.c_str(), " ", 0, true);
		// If the user specified name
		if(params.size() == 1)
		{
			// If they are an admin
			if(isAdmin(playerID))
			{
				// Get other player's id by callsign
				int otherPlayerID = getPlayerByCallsign(params.get(0));

				// We have a valid player
				if(otherPlayerID != -1)
				{
					// Fetch the data
					bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
					// Kill the player
					bz_killPlayer(otherPlayerID,1,-1);
					// Send a message to the player
					bz_sendTextMessagef(BZ_SERVER, otherPlayerID, "Your spawn position was reset by %s", player->callsign.c_str());
					// Send a message to the reseter
					bz_sendTextMessagef(BZ_SERVER, playerID, "%s's spawn position has been reset", params.get(0).c_str());
					// Send a message to the admin channel
					bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s's spawn position was reset by %s", params.get(0).c_str(), player->callsign.c_str());
					// Find their spawnpoints
					std::map<PlayerID, SpawnPoint>::iterator sPoint = spawnPoints.find(otherPlayerID);
					// Erase their data
					if(sPoint != spawnPoints.end()){
						spawnPoints.erase(sPoint);
					}
					// Clear up the record
					bz_freePlayerRecord(player);

				} else {
					// Player does not exist
					bz_sendTextMessagef(BZ_SERVER, playerID, "No Such Player: %s", params.get(0).c_str());
				}
			} else {
				// Not an admin
				bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to reset others spawn positions");
			}
		} else {
			// You want to reset you own spawnpoint, kill youself
			bz_killPlayer(playerID,1,-1);
			// Let you know it was reset
			bz_sendTextMessage(BZ_SERVER, playerID, "Your spawn position has been reset");
			// Find your spawnpoints
			std::map<PlayerID, SpawnPoint>::iterator sPoint = spawnPoints.find(playerID);
			// Erase your data
			if(sPoint != spawnPoints.end()){
				spawnPoints.erase(sPoint);
			}
		}
		return true;
	}
	else if(command == "spawnresetall")
	{
		// Fetch the data
		bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
		// If user is an admin
		if ( isAdmin(playerID) )
		{
			// Clear the spawn positions
			spawnPoints.clear();
			// Format the message
			std::string msg = player->callsign.c_str();
			msg += " has reset all spawn positions";
			// Send the message
			bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,msg.c_str());
			// Get a list of all players
			bz_APIIntList *playerList = bz_newIntList();
			bz_getPlayerIndexList ( playerList );
			// For every player, kill them
			for ( unsigned int i = 0; i < playerList->size(); i++ )
				bz_killPlayer(playerList->get(i),false);
			// Free the record
			bz_freePlayerRecord(player);
			// Delete the list
			bz_deleteIntList(playerList);

			return true;
		} else {
			// Player is not an admin
			bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permissions to reset all spawn positions");
		}
	}
	return true;
}

void CheckpointHandler::Cleanup()
{
	Flush();
	bz_removeCustomMapObject("CHECKPOINT");
	bz_removeCustomSlashCommand ("spawnreset");
	bz_removeCustomSlashCommand ("spawnresetall");
	bz_debugMessage(4,"Checkpoint plugin unloaded");
}

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
