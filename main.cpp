#include <algorithm>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <memory>
#include <SFML/Graphics.hpp>
#include <string>
#include <thread>
#include <vector>

#include <cstdio>
#include <cstdlib>

#include "easywsclient/easywsclient.hpp"
#include "easywsclient/easywsclient.cpp"

bool closingApp = false;
bool darkTheme = false;
bool ejectMass = false;
bool sendName = false;
bool sendReset = false;
bool spectate = false;
bool updateMouse = false;

double clientMouseX;
double clientMouseY;

double worldLeft;
double worldTop;
double worldRight;
double worldBottom;

unsigned int windowWidth = 1920;
unsigned int windowHeight = 1080;

uint32_t clientID = 0;

std::string connectionString;

struct cameraStruct
{
	double Left = 0.d;
	double Right = (double)windowWidth;
	double Top = 0.d;
	double Bottom = (double)windowHeight;

	double CenterX = windowWidth / 2.d;
	double CenterY = windowHeight / 2.d;

	double Width = (double)windowWidth;
	double Height = (double)windowHeight;
};

struct nodeDataStruct
{
	uint32_t ID = 0;
	uint16_t X = 0;
	uint16_t Y = 0;
	uint16_t Radius = 0;
	uint8_t Red = 0;
	uint8_t Green = 0;
	uint8_t Blue = 0;
	sf::String Name;

	bool IsVirus = false;
	bool IsAgitated = false;
};

struct leaderboardStruct
{
	std::vector<uint32_t> ID;
	std::vector<sf::String> Name;
};

sf::String playerNameString = "Ogar Client";

cameraStruct cameraData;
leaderboardStruct leaderboardData;

std::vector<nodeDataStruct> nodeData;
std::vector<nodeDataStruct> virusData;
std::vector<nodeDataStruct> clientData;

std::vector<uint8_t> playerMouseMessage;
std::vector<uint8_t> playerName;

void printMessage(std::vector<uint8_t> message, bool hex)
{
	std::cout << std::endl << std::dec << message.size() << std::endl;

	if (hex)
	{
		std::cout << std::hex;
	}

	for (unsigned int i = 0; i < message.size(); i++)
	{
		std::cout << std::setw(2) << std::setfill('0') << static_cast<int>(message[i]);
	}

	std::cout << std::dec << "\n\n";

	return;
}

void decodeLeaderboardUpdate(std::vector<uint8_t> message)
{
	uint16_t nameChar;
	uint32_t length;
	uint32_t tempID;
	unsigned int bufferOffset = 1;

	memcpy(&length, message.data() + bufferOffset, sizeof(length));
	bufferOffset += sizeof(length);

	leaderboardData.ID.clear();
	leaderboardData.Name.clear();

	for (unsigned int iter = 0; iter < length; iter++)
	{
		std::vector<uint8_t> nameBuffer;

		memcpy(&tempID, message.data(), sizeof(tempID));
		bufferOffset += sizeof(tempID);

		leaderboardData.ID.push_back(tempID);

		while (true)
		{
			memcpy(&nameChar, message.data() + bufferOffset, sizeof(nameChar));
			bufferOffset += sizeof(nameChar);

			nameBuffer.push_back(nameChar);

			if (nameChar == 0)
			{
				leaderboardData.Name.push_back(sf::String::fromUtf16(nameBuffer.begin(), nameBuffer.end()));

				break;
			}
		}
	}

	return;
}

unsigned int decodeNodeID(std::vector<uint8_t> message)
{
	unsigned int NodeID = 0;

	memcpy(&NodeID, message.data() + 1, 4);

	return NodeID;
}

void decodeNodeUpdate(std::vector<uint8_t> message)
{
	nodeDataStruct tempNode;
	unsigned int bufferOffset = 1; // Skip the packet ID (UINT8)

	uint8_t flags;
	uint16_t nodesToDelete;
	uint32_t killerNodeID;
	uint32_t killeeNodeID;


	memcpy(&nodesToDelete, message.data() + bufferOffset, sizeof(nodesToDelete));
	bufferOffset += sizeof(nodesToDelete);

	for (unsigned int deleteIter = 0; deleteIter < nodesToDelete; deleteIter++)
	{
		memcpy(&killerNodeID, message.data() + bufferOffset, sizeof(killerNodeID));
		bufferOffset += sizeof(killerNodeID);

		memcpy(&killeeNodeID, message.data() + bufferOffset, sizeof(killeeNodeID));
		bufferOffset += sizeof(killeeNodeID);

		for (unsigned int nodeIter = 0; nodeIter < nodeData.size(); nodeIter++)
		{
			if (killeeNodeID == nodeData[nodeIter].ID)
			{
				nodeData.erase(nodeData.begin() + nodeIter);

				break;
			}
		}
	}

	while (true)
	{
		bool nodeExists = false;
		unsigned int nodeIndex;
		uint16_t nameChar;
		std::vector<uint16_t> nameMessage;

		memcpy(&tempNode.ID, message.data() + bufferOffset, sizeof(tempNode.ID));
		bufferOffset += sizeof(tempNode.ID);

		memcpy(&tempNode.X, message.data() + bufferOffset, sizeof(tempNode.X));
		bufferOffset += sizeof(tempNode.X);

		memcpy(&tempNode.Y, message.data() + bufferOffset, sizeof(tempNode.Y));
		bufferOffset += sizeof(tempNode.Y);

		memcpy(&tempNode.Radius, message.data() + bufferOffset, sizeof(tempNode.Radius));
		bufferOffset += sizeof(tempNode.Radius);

		memcpy(&tempNode.Red, message.data() + bufferOffset, sizeof(tempNode.Red));
		bufferOffset += sizeof(tempNode.Red);

		memcpy(&tempNode.Green, message.data() + bufferOffset, sizeof(tempNode.Green));
		bufferOffset += sizeof(tempNode.Green);

		memcpy(&tempNode.Blue, message.data() + bufferOffset, sizeof(tempNode.Blue));
		bufferOffset += sizeof(tempNode.Blue);

		memcpy(&flags, message.data() + bufferOffset, sizeof(flags));
		bufferOffset += sizeof(flags);

		if (flags != 0)
		{
			tempNode.IsVirus = flags & 1;
			bufferOffset += 4 * ((flags >> 1) & 1);
			bufferOffset += 8 * ((flags >> 3) & 1);
			tempNode.IsAgitated = (flags >> 4) & 1;
			bufferOffset += 4 * ((flags >> 7) & 1);
		}

		while (true)
		{
			memcpy(&nameChar, message.data() + bufferOffset, sizeof(nameChar));
			bufferOffset += sizeof(nameChar);

			nameMessage.push_back(nameChar);

			if (nameChar == 0)
			{
				tempNode.Name = sf::String::fromUtf16(nameMessage.begin(), nameMessage.end());

				break;
			}
		}

		if (tempNode.ID == 0)
		{
			break;
		}

		if (tempNode.IsVirus)
		{
			for (unsigned int virusIter = 0; virusIter < virusData.size(); virusIter++)
			{
				if (virusData[virusIter].ID == tempNode.ID)
				{
					nodeExists = true;
					nodeIndex = virusIter;

					break;
				}
			}

			if (nodeExists)
			{
				virusData[nodeIndex] = tempNode;
			}
			else
			{
				virusData.push_back(tempNode);
			}
		}
		else
		{
			for (unsigned int nodeIter = 0; nodeIter < nodeData.size(); nodeIter++)
			{
				if (nodeData[nodeIter].ID == tempNode.ID)
				{
					nodeExists = true;
					nodeIndex = nodeIter;

					break;
				}
			}

			if (nodeExists)
			{
				nodeData[nodeIndex] = tempNode;
			}
			else
			{
				nodeData.push_back(tempNode);
			}
		}
	}

	return;
}

void decodeWorldMessage(std::vector<uint8_t> message)
{
	memcpy(&worldBottom, message.data() + 25, 8);
	memcpy(&worldRight, message.data() + 17, 8);
	memcpy(&worldTop, message.data() + 9, 8);
	memcpy(&worldLeft, message.data() + 1, 8);

	return;
}

void encodeClientMouse()
{
	unsigned int bufferOffset = 1;
	unsigned int blanking = 0;

	playerMouseMessage.clear();
	playerMouseMessage.resize(21);

	playerMouseMessage[0] = 0x10; // PacketID

	memcpy(playerMouseMessage.data() + bufferOffset, &clientMouseX, sizeof(clientMouseX));
	bufferOffset += sizeof(clientMouseX);

	memcpy(playerMouseMessage.data() + bufferOffset, &clientMouseY, sizeof(clientMouseY));
	bufferOffset += sizeof(clientMouseY);

	memcpy(playerMouseMessage.data() + bufferOffset, &blanking, sizeof(blanking));
	return;
}

void encodePlayerName()
{
	playerName.clear();
	playerName.push_back(00);
	playerName.resize(playerNameString.getSize() + 1);

	memcpy(playerName.data() + 1, playerNameString.getData(), playerNameString.getSize());

	return;
}

float radiusToMass(float radius)
{
	return radius * radius / 100.0;
}

void updateCamera()
{
	cameraStruct defaultCamera;

	defaultCamera.Left = 0.d;
	defaultCamera.Right = worldRight;
	defaultCamera.Top = 0.d;
	defaultCamera.Bottom = worldBottom;
	defaultCamera.CenterX = worldRight / 2.d;
	defaultCamera.CenterY = worldBottom / 2.d;
	defaultCamera.Width = worldRight;
	defaultCamera.Height = worldBottom;

	unsigned int clientIndex = 0;
	double totalRadius = 0.d;
	double range = 0.d;

	for (unsigned int nodeIter = 0; nodeIter < nodeData.size(); nodeIter++)
	{
		if (nodeData[nodeIter].ID == clientID)
		{
			clientIndex = nodeIter;

			break;
		}
	}


	if (clientIndex == 0)
	{
		cameraData = defaultCamera;

		return;
	}

	cameraData.CenterX = (double)nodeData[clientIndex].X;
	cameraData.CenterY = (double)nodeData[clientIndex].Y;

	totalRadius += (double)nodeData[clientIndex].Radius;

	range = totalRadius * 25.d;

	if (windowWidth > windowHeight)
	{
		cameraData.Left = (double)nodeData[clientIndex].X - range;
		cameraData.Right = (double)nodeData[clientIndex].X + range;

		cameraData.Top = (double)nodeData[clientIndex].Y - (range * ((double)windowHeight / (double)windowWidth));
		cameraData.Bottom = (double)nodeData[clientIndex].Y + (range * ((double)windowHeight / (double)windowWidth));
	}
	else
	{
		cameraData.Top = (double)nodeData[clientIndex].Y - range;
		cameraData.Bottom = (double)nodeData[clientIndex].Y + range;

		cameraData.Left = (double)nodeData[clientIndex].X - (range * (windowWidth / windowHeight));
		cameraData.Right = (double)nodeData[clientIndex].X + (range * (windowWidth / windowHeight));
	}

	cameraData.Height = cameraData.Bottom - cameraData.Top;
	cameraData.Width = cameraData.Right - cameraData.Left;

	return;
}

void OAClient()
{
	sf::RenderWindow window(sf::VideoMode(windowWidth, windowHeight), "Open Agar Client");
	sf::RenderTexture leaderboardTexture;
	window.setVerticalSyncEnabled(true);

	sf::Font ubuntuFont;
	if(!ubuntuFont.loadFromFile("Ubuntu-B.ttf"))
	{
		"Please keep the Ubunt-B.tff file in the program directory.\n";
	}

	sf::Text text;
	text.setFont(ubuntuFont);
	text.setColor(sf::Color(255, 255, 255, 255));
	text.setStyle(sf::Text::Bold);

	sf::CircleShape circle(1.f);
	circle.setPointCount(100);
	sf::Color circleColor = sf::Color(255, 255, 255, 255);
	sf::Color fillColor;
	sf::Color strokeColor;

	sf::RectangleShape leaderboardRect;
	leaderboardRect.setFillColor(sf::Color(0, 0, 0, 102));
	leaderboardRect.setSize(sf::Vector2f(200.f, 300.f));

	while (window.isOpen())
	{
		sf::Event event;
		float textYOffset = -10.f;

		updateCamera();

		window.setView(sf::View(sf::FloatRect(cameraData.Left, cameraData.Top, cameraData.Width, cameraData.Height)));

		sf::Vector2f mousePosition = window.mapPixelToCoords(sf::Mouse::getPosition(window));

		clientMouseX = (double)mousePosition.x;
		clientMouseY = (double)mousePosition.y;

		encodeClientMouse();

		while (window.pollEvent(event))
		{
			switch (event.type)
			{
				case sf::Event::Closed:
					window.close();

					break;

				case sf::Event::KeyPressed:
					sendName = event.key.code == sf::Keyboard::Space;

					switch (event.key.code)
					{
						case sf::Keyboard::Space:
							sendName = true;
							updateMouse = true;

							break;

						case sf::Keyboard::Home:
							spectate = true;
							nodeData.clear();

							break;

						case sf::Keyboard::End:
							sendReset = true;
							nodeData.clear();

							break;

						case sf::Keyboard::Escape:
							window.close();

							break;

						case sf::Keyboard::W:
							ejectMass = true;

							break;

						case sf::Keyboard::Up:
							//

						default:
							break;
					}

					break;

				case sf::Event::Resized:
					windowWidth = event.size.width;
					windowHeight = event.size.height;

				default:
					break;
			}

		}

		if (darkTheme)
		{
			fillColor = sf::Color(17, 17, 17, 255);
			strokeColor = sf::Color(170, 170, 170, 204);
		}
		else
		{
			fillColor = sf::Color(242, 251, 255, 255);
			strokeColor = sf::Color(170, 170, 170, 204);
		}

		window.clear(fillColor);

		for (unsigned int nodeIter = 0; nodeIter < nodeData.size(); nodeIter++) // Then draw players and food
		{
			circle.setPosition((float)nodeData[nodeIter].X - (float)nodeData[nodeIter].Radius, (float)nodeData[nodeIter].Y - (float)nodeData[nodeIter].Radius);
			circle.setRadius((float)nodeData[nodeIter].Radius);

			circleColor.r = nodeData[nodeIter].Red;
			circleColor.g = nodeData[nodeIter].Green;
			circleColor.b = nodeData[nodeIter].Blue;

			circle.setFillColor(circleColor);

			window.draw(circle);

			if (nodeData[nodeIter].Name.getSize() > 0)
			{
				text.setCharacterSize(10);
				text.setString(nodeData[nodeIter].Name);
				text.setPosition((float)nodeData[nodeIter].X - text.getLocalBounds().width / 2.f, (float)nodeData[nodeIter].Y);

				window.draw(text);
			}
		}

		for (unsigned int virusIter = 0; virusIter < virusData.size(); virusIter++) // Draw Viri
		{
			circle.setPosition((float)virusData[virusIter].X - (float)virusData[virusIter].Radius, (float)virusData[virusIter].Y - (float)virusData[virusIter].Radius);
			circle.setRadius((float)virusData[virusIter].Radius);

			circleColor.r = virusData[virusIter].Red;
			circleColor.g = virusData[virusIter].Green;
			circleColor.b = virusData[virusIter].Blue;

			circle.setFillColor(circleColor);

			window.draw(circle);
		}

		// Draw leaderboard
		window.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)windowWidth, (float)windowHeight)));
		leaderboardRect.setPosition((float)windowWidth - 200.f, textYOffset);
		window.draw(leaderboardRect);

		text.setCharacterSize(30);
		text.setString("Leaderboard");
		text.setPosition((float)windowWidth - 200.f + (200.f - text.getLocalBounds().width) / 2.f, 0.f);

		textYOffset += text.getLocalBounds().height + 20.f;
		window.draw(text);

		text.setCharacterSize(20);

		for (unsigned int leaderIter = 0; leaderIter < leaderboardData.ID.size(); leaderIter++)
		{
			text.setString(std::to_string(leaderIter) + ". " + leaderboardData.Name[leaderIter]);
			text.setPosition((float)windowWidth - 200.f + (200.f - text.getLocalBounds().width) / 2.f, textYOffset);

			textYOffset += text.getLocalBounds().height + 7.f;
			window.draw(text);
		}

		window.display();

		std::this_thread::sleep_for(std::chrono::microseconds(101)); // Enough time to allow for the web sockets to always update
	}

	closingApp = true;

	return;
}

void WSClient()
{
	std::unique_ptr<easywsclient::WebSocket> ws(easywsclient::WebSocket::from_url(connectionString));
	std::vector<uint8_t> inMessage;

	ws->sendBinary(std::vector<uint8_t>({0xFF, 0x01, 0x00, 0x00, 0x00})); // Handshake

	while (ws->getReadyState() != easywsclient::WebSocket::CLOSED) // Websocket connection loop
	{
		bool gotMessage = false;

		ws->poll();
		ws->dispatchBinary([gotMessageOut=&gotMessage, messageOut=&inMessage, ws=ws.get()](const std::vector<uint8_t>&inMessage)
		{
			*gotMessageOut = true;
			*messageOut = inMessage;
		});

		if (gotMessage) // I hope you're hungry because here are the meat and potatoes
		{
			switch(static_cast<int>(inMessage[0]))
			{
				case 16: // Node update
					decodeNodeUpdate(inMessage);
					break;

				case 17: // Update client in spectator mode
					//std::cout << "Unhandled spectator client update\n";
					//clientNodeUpdate(inMessage);

					break;

				case 32: // New client location
					clientID = decodeNodeID(inMessage);
					std::cout << "Client update\n";

					break;


				case 49: // (FFA) Leaderboard Update
					decodeLeaderboardUpdate(inMessage);
					break;

				case 50: // (Team) Leaderboard Update

					break;

				case 64: // World size message
					decodeWorldMessage(inMessage);
					std::cout << "World update\n";

					break;

				default:
					printMessage(inMessage, true);
					std::cout << "Unknown\n";

					break;
			}

			if (ejectMass)
			{
				ws->sendBinary(std::vector<uint8_t>({0x15}));
				std::cout << "Ejected mass\n";
			}

			if (spectate)
			{
				ws->sendBinary(std::vector<uint8_t>({0x01}));
				std::cout << "Spectating\n";
				spectate = false;
			}

			if (sendName)
			{
				encodePlayerName();
				ws->sendBinary(playerName);
				std::cout << "Sent name\n";
				sendName = false;
			}

			if (sendReset)
			{
				ws->sendBinary(std::vector<uint8_t>({0xFF, 0x01, 0x00, 0x00, 0x00}));
				std::cout << "Sent reset\n";
				sendReset = false;
			}

			if (updateMouse)
			{
				ws->sendBinary(playerMouseMessage);
			}
		}

		if (closingApp)
		{
			ws->close();

			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	ws->close();

	return;
}

int main(int argc, char* argv[])
{
	std::cout << argc << std::endl;
	if (argc == 2)
	{
		connectionString = argv[1];
	}
	else
	{
		std::cout << "Please launch in the form \"Open Agar Client.exe\" ws://server:port\n";
		return 1;
	}

	std::thread WSClientThread(WSClient);
	std::thread OAClientThread(OAClient);

	WSClientThread.join();
	OAClientThread.join();

	return 0;
}
