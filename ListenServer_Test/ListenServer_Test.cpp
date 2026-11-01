#pragma comment(lib, "ws2_32.lib")

#include "framework.h"
#include "ListenServer_Test.h"

#define MAX_LOADSTRING 100


HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    ServerWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    ClientWndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

SOCKET listenSock;
SOCKET HostSock;
SOCKET ClientSock;

DWORD WINAPI AcceptThread(LPVOID);
DWORD WINAPI ClientRecvThread(LPVOID lpParam);
DWORD WINAPI ServerRecvThread(LPVOID lpParam);

vector<SOCKET> v_SockArr;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);


	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_LISTENSERVERTEST, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LISTENSERVERTEST));
	MSG msg;
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa); //윈도우 소켓시스템 On

	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

//사용할 윈도우 클래스를 등록
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LISTENSERVERTEST));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	// 서버창 클래스 등록
	wcex.lpfnWndProc = ServerWndProc;
	wcex.lpszClassName = L"ServerWindowClass";
	RegisterClassExW(&wcex);

	// 클라이언트창 클래스 등록
	wcex.lpfnWndProc = ClientWndProc;
	wcex.lpszClassName = L"ClientWindowClass";
	RegisterClassExW(&wcex);

	return TRUE;
}

//창 2개 생성
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hServerWnd = CreateWindowW(
		L"ServerWindowClass",
		L"서버창",
		WS_OVERLAPPEDWINDOW,
		100, 100, 400, 300,
		nullptr, nullptr, hInstance, nullptr);

	HWND hClientWnd = CreateWindowW(
		L"ClientWindowClass",
		L"클라이언트창",
		WS_OVERLAPPEDWINDOW,
		550, 100, 400, 300,
		nullptr, nullptr, hInstance, nullptr);


	if (!hServerWnd || !hClientWnd)  return FALSE;


	ShowWindow(hServerWnd, nCmdShow);
	UpdateWindow(hServerWnd);

	ShowWindow(hClientWnd, nCmdShow);
	UpdateWindow(hClientWnd);

	return TRUE;
}

LRESULT CALLBACK ServerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
		HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);

		// 방 생성 버튼 1001
		CreateWindowW(
			L"BUTTON", L"방 생성",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			10, 10, 100, 30,
			hWnd, (HMENU)1001, hInst, NULL);

		// 채팅 로그창 (멀티라인 + 자동스크롤 + 읽기전용) 1002
		CreateWindowW(
			L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_BORDER |
			ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
			10, 50, 360, 180,
			hWnd, (HMENU)1002, hInst, NULL);

		// 메시지 입력창 1003
		CreateWindowW(
			L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_BORDER,
			10, 240, 260, 25,
			hWnd, (HMENU)1003, hInst, NULL);

		// 보내기 버튼 1004
		CreateWindowW(
			L"BUTTON", L"보내기",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			280, 240, 90, 25,
			hWnd, (HMENU)1004, hInst, NULL);

		break;
	}

	case WM_COMMAND:
	{
		int id = LOWORD(wParam);
		if (LOWORD(wParam) == 1001) // 방 생성 버튼
		{
			listenSock = socket(AF_INET, SOCK_STREAM, 0); //소켓생성. AF_INET: IPv4, SOCK_STREAM->TCP, 0 ->기본 프로토콜(TCP)

			sockaddr_in addr = {};
			addr.sin_family = AF_INET; //IPv4 주소체계 사용
			addr.sin_port = htons(9000); //포트번호 9000
			addr.sin_addr.s_addr = htonl(INADDR_ANY); //내 컴퓨터의 모든 IP에서 접속허용

			bind(listenSock, (sockaddr*)&addr, sizeof(addr)); //소켓에 주소를 바인딩
			listen(listenSock, SOMAXCONN); //소켓을 대기상태로 전환

			CreateThread(NULL, 0, AcceptThread, NULL, 0, NULL); //연결을 받을 전용쓰레드를 새로 생성

			MessageBox(hWnd, L"방생성됨", L"서버", MB_OK);
		}
		else if (id == 1004) // 보내기 버튼
		{
			char buf[256];
			GetWindowTextA(GetDlgItem(hWnd, 1003), buf, 256); // 입력창에서 텍스트 가져오기
			send(HostSock, buf, strlen(buf), 0); //호스트소켓과 연결된 클라이언트로 보냄. 이후 recv를 통해 받음

			// 서버 로그창에 출력
			HWND hLog = GetDlgItem(hWnd, 1002);
			int len = GetWindowTextLengthA(hLog);

			// 기존 로그 뒤에 메시지 추가
			SendMessageA(hLog, EM_SETSEL, len, len);
			SendMessageA(hLog, EM_REPLACESEL, FALSE, (LPARAM)buf);
			SendMessageA(hLog, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
		}

		break;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK ClientWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
		HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);

		// 접속 버튼 (서버의 "방 생성"과 대응)
		CreateWindowW(
			L"BUTTON", L"서버 접속",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			10, 10, 100, 30,
			hWnd, (HMENU)2001, hInst, NULL);

		// 채팅 로그창 (서버와 동일)
		CreateWindowW(
			L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_BORDER |
			ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
			10, 50, 360, 180,
			hWnd, (HMENU)2002, hInst, NULL);

		// 메시지 입력창 (서버와 동일)
		CreateWindowW(
			L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_BORDER,
			10, 240, 260, 25,
			hWnd, (HMENU)2003, hInst, NULL);

		// 보내기 버튼 (서버와 동일)
		CreateWindowW(
			L"BUTTON", L"보내기",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			280, 240, 90, 25,
			hWnd, (HMENU)2004, hInst, NULL);

		break;
	}

	case WM_COMMAND:
	{
		int id = LOWORD(wParam);

		if (id == 2001) // 서버 접속 버튼
		{
			ClientSock = socket(AF_INET, SOCK_STREAM, 0);

			sockaddr_in addr = {};
			addr.sin_family = AF_INET;
			addr.sin_port = htons(9000);
			InetPton(AF_INET, L"127.0.0.1", &addr.sin_addr); //127.0.0.1 ip주소를 addr에 저장
			int ret = connect(ClientSock, (sockaddr*)&addr, sizeof(addr));

			if (ret == SOCKET_ERROR) MessageBox(hWnd, L"서버 접속 실패", L"클라이언트", MB_OK);
			else
			{
				MessageBox(hWnd, L"서버 접속 성공", L"클라이언트", MB_OK);
				CreateThread(NULL, 0, ClientRecvThread, (LPVOID)hWnd, 0, NULL);
			}
		}

		else if (id == 2004) // 보내기 버튼
		{
			char buf[256];
			GetWindowTextA(GetDlgItem(hWnd, 2003), buf, 256); // 입력창에서 텍스트 가져오기
			send(ClientSock, buf, strlen(buf), 0);

			// 서버 로그창에 출력
			HWND hLog = GetDlgItem(hWnd, 2002);
			int len = GetWindowTextLengthA(hLog);

			// 기존 로그 뒤에 메시지 추가
			SendMessageA(hLog, EM_SETSEL, len, len);
			SendMessageA(hLog, EM_REPLACESEL, FALSE, (LPARAM)buf);
			SendMessageA(hLog, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
		}
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	}
	return DefWindowProc(hWnd, message, wParam, lParam);

}

DWORD WINAPI AcceptThread(LPVOID)//3way HandShake를 통한 환영소켓생성만 담당
{
	while (true)
	{
		// listenServer로 accept발생시 새로운 소켓생성후(환영소켓) host로 할당. 
		// 설정값은 ListenServer값을 복사하고 연결이 들어온 소켓과 1:1 대응
		HostSock = accept(listenSock, NULL, NULL);

		//호스트는 매번 덮어씌어지므로 대상들을 벡터로 저장
		v_SockArr.push_back(HostSock);
		MessageBox(NULL, L"클라이언트 접속됨", L"서버", MB_OK);

		CreateThread(NULL, 0, ServerRecvThread, (LPVOID)HostSock, 0, NULL);
		//4번째 인자는 생성할 쓰레드의인자값. 무조건 LPVOID타입만 받는다
	}
	return 0;
}

DWORD WINAPI ServerRecvThread(LPVOID lpParam)
{
	SOCKET sock = (SOCKET)lpParam; //데이터를 받는 소켓
	char buf[256];

	HWND hServerWnd = FindWindowW(L"ServerWindowClass", NULL); //서버 윈도우창 탐색
	HWND hLog = GetDlgItem(hServerWnd, 1002); //윈도우창내부의 로그창 탐색

	while (true)
	{
		int ret = recv(sock, buf, sizeof(buf) - 1, 0); //sock으로 데이터가 들어올때까지 블로킹
		if (ret <= 0) break;

		//데이터는 비트단위로 들어오기에 마지막에 널문자는 수동으로 삽입해야함
		buf[ret] = 0;

		int len = GetWindowTextLengthA(hLog); //커서의 끝위치를 확인
		SendMessageA(hLog, EM_SETSEL, len, len); //커서를 len,len위치로 이동 EM_SETSEL은 선택영역 지정
		SendMessageA(hLog, EM_REPLACESEL, FALSE, (LPARAM)buf); //현재 커서위치에 buf 문자열 삽입
		SendMessageA(hLog, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");//줄바꿈 추가
		//EM_REPLACESEL은 선택영역에 텍스트 삽입 FALSE는 선택된 텍스트를 지울지 말지
	}

	closesocket(sock);
	return 0;
}

DWORD WINAPI ClientRecvThread(LPVOID lpParam)
{
	HWND hClientWnd = (HWND)lpParam;
	HWND hLog = GetDlgItem(hClientWnd, 2002);

	char buf[256];

	while (true)
	{
		//ret == 0 연결종료. ret > 0 정상메시지. ret < 0 오류 -> break
		int ret = recv(ClientSock, buf, sizeof(buf) - 1, 0); //받은 바이트수를 반환
		if (ret <= 0)
			break;

		buf[ret] = 0;//끝에 널문자

		// 로그창에 출력
		int len = GetWindowTextLengthA(hLog);
		SendMessageA(hLog, EM_SETSEL, len, len);
		SendMessageA(hLog, EM_REPLACESEL, FALSE, (LPARAM)buf);
		SendMessageA(hLog, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
	}

	return 0;
}
