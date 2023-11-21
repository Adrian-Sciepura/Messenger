.686
.model flat, c

extern ExitProcess@4	: PROC

extern WSAGetLastError@0 : PROC
extern WSACleanup@0		: PROC
extern WSAStartup@8		: PROC

extern closesocket@4	: PROC
extern socket@12		: PROC

extern connect@12		: PROC
extern send@16			: PROC
extern recv@16			: PROC

extern inet_addr@4		: PROC
extern htons@4			: PROC

extern printf			: PROC
extern fgets			: PROC
extern __acrt_iob_func	: PROC
extern strcspn			: PROC
extern strlen			: PROC
extern sprintf			: PROC
extern _beginthread		: PROC
extern _endthread		: PROC

public main

.data
; connect data
wsaData				db  400 dup (0)
clientSocket		db	4	dup (0)
serverAddr			db  16	dup (0)

; user data
username			db	16+1 dup (0)
sendStorage			db	1024 dup (0)
inputBuffer			db	1024 dup (0)
receiveBuffer		db	1024 dup (0)

; const values
NEW_LINE			db	010H
PACKAGE_HEADER		db	'msg', 00H
serverIP			db	'127.0.0.1', 00H
serverPORT			dw	25565
bufferSize			dw	1024
INVALID_SOCKET		dd	-1;0FFFFH
SOCKET_ERROR		dd	-1

; streams
stdin				dd	0

;messages
sendPackageformat	db	'%s%s', 00H
recvMessageformat	db	0DH, '%s', 0AH, '[You]: ', 00H
userInput			db	'[You]: ', 00H
enterName			db	'Enter your username (max 16 characters): ', 00H

; error messages
error_wsaDataSetup	db	'Failed to initialize winsock', 0AH, 00H
error_socketSetup	db	'Failed to create socket', 0AH, 00H
error_connection	db	'Connection failed', 0AH, 00H
error_disconnected	db	'Server disconnected', 0AH, 00H

; success messages
success_connected	db	'Connected to the server', 0AH, 00H


.code

sendPackageToServer PROC

	push ebp
	mov ebp, esp
	push ebx
	push ecx

	push DWORD PTR [ebp + 12]
	push OFFSET PACKAGE_HEADER
	push OFFSET sendPackageformat
	push OFFSET sendStorage
	call sprintf
	add esp, 16

send:
	mov ecx, [ebp + 16]
	add ecx, 3

	push 0								; flags
	push ecx							; content length
	push OFFSET sendStorage
	push [ebp + 8]
	call send@16

	cmp eax, SOCKET_ERROR
	jne success

error:
	mov eax, 0
	jmp epilog

success:
	mov eax, 1

epilog:
	pop ecx
	pop ebx
	pop ebp
	ret

sendPackageToServer ENDP



receiveMessages PROC

	push ebp
	mov ebp, esp
	push ebx
	push ecx

	mov ebx, [ebp + 8]						; socket

receiveMessageLoop:
	push 0
	push 1024
	push OFFSET receiveBuffer
	push ebx
	call recv@16

	cmp eax, 0
	jbe disconected

	mov receiveBuffer[eax], 00H
	
	push OFFSET receiveBuffer
	push OFFSET recvMessageformat
	call printf
	add esp, 8
	jmp receiveMessageLoop

disconected:
	push OFFSET error_disconnected
	call printf
	add esp, 4

	call _endthread

	pop ecx
	pop ebx
	pop ebp
	ret

receiveMessages ENDP



main PROC

stdinSetup:
	push 0
	call __acrt_iob_func
	add esp, 4
	mov stdin, eax

wsaDataSetup:
	push DWORD PTR OFFSET wsaData
	push 514								; 00000202H
	call WSAStartup@8

	cmp eax, 0
	jz clientSocketSetup

	push OFFSET error_wsaDataSetup
	call printf
	add esp, 4
	jmp errorOccured

clientSocketSetup:
	push 0									; protocol
	push 1									; SOCK_STREAM
	push 2									; AF_INET
	call socket@12

	mov DWORD PTR clientSocket, eax
	cmp DWORD PTR clientSocket, -1
	jne serverAddrSetup

	push OFFSET error_socketSetup
	call printf
	add esp, 4
	jmp errorOccured_cleanup
	
serverAddrSetup:
	mov eax, 2								; AF_INET
	mov WORD PTR serverAddr[0], ax			; serverAddr.sin_family

	push 25565
	call htons@4
	mov WORD PTR serverAddr[2], ax			; serverAddr.sin_port

	push OFFSET serverIP
	call inet_addr@4
	mov DWORD PTR serverAddr[4], eax		; serverAddr.sin_addr.s_addr

connect:
	push 16									; sizeof serverAddr
	push OFFSET serverAddr
	push DWORD PTR clientSocket
	call connect@12

	cmp eax, -1
	jne getUsername

	push OFFSET error_connection
	call printf
	add esp, 4
	jmp errorOccured_closeSocket

getUsername:
	push OFFSET success_connected
	call printf
	add esp, 4

	push OFFSET enterName
	call printf
	add esp, 4

	push stdin								; input stream
	push 16									; characters to get
	push OFFSET username					; buffer
	call fgets
	add esp, 12

	push OFFSET NEW_LINE					; new line symbol
	push OFFSET username					; buffer
	call strcspn
	add esp, 8

	mov username[eax], 0					; replace \n with \0

sendUserNameToServer:
	push OFFSET username
	call strlen
	add esp, 4

	push eax
	push OFFSET username
	push DWORD PTR clientSocket
	call sendPackageToServer
	add esp, 12

	cmp eax, 0
	je errorOccured_closeSocket

	push DWORD PTR clientSocket
	push 0
	push OFFSET receiveMessages
	call _beginthread
	add esp, 12


sendMessageLoop:
	push OFFSET userInput
	call printf
	add esp, 4

	push stdin
	push 1024
	push OFFSET inputBuffer
	call fgets
	add esp, 12

	push OFFSET inputBuffer
	call strlen
	add esp, 4

	push eax
	push OFFSET inputBuffer
	push DWORD PTR clientSocket
	call sendPackageToServer
	add esp, 12

	cmp eax, 1
	je sendMessageLoop


errorOccured_closeSocket:
	push DWORD PTR clientSocket
	call closesocket@4

errorOccured_cleanup:
	call WSACleanup@0

errorOccured:
	push 1
	call ExitProcess@4

main ENDP
END