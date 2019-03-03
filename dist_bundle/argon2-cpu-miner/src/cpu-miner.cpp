#include <stdio.h>

#include <iostream>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <ctime>
#include <vector>

#include <csignal>

#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>


#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "argon2.h"


using namespace std;

static uint64_t rdtsc(void) {
#ifdef _MSC_VER
    return __rdtsc();
#else
#if defined(__amd64__) || defined(__x86_64__)
    uint64_t rax, rdx;
    __asm__ __volatile__("rdtsc" : "=a"(rax), "=d"(rdx) : :);
    return (rdx << 32) | rax;
#elif defined(__i386__) || defined(__i386) || defined(__X86__)
    uint64_t rax;
    __asm__ __volatile__("rdtsc" : "=A"(rax) : :);
    return rax;
#else
#error "Not implemented!"
#endif
#endif
}

#define CONST_CAST(x) (x)(uintptr_t)

namespace
{
	typedef uintmax_t nanosecs;
	typedef std::chrono::steady_clock clock_type;

	std::vector<std::thread> threads;
	std::condition_variable new_data;
	std::mutex data_mutex;
	bool data_loaded = false;
	std::mutex input_mutex, output_mutex;

	bool program_done = false;
	volatile std::sig_atomic_t gSignalStatus;
	
	long unsigned g_id = 0;
    uint32_t g_startPrev = 0, g_start = 0, g_length = 0, g_end = 0, g_working = 0;

    unsigned char g_pwd[1024 * 1027], g_difficulty[34];
    unsigned char tmp_pwd[1024 * 1027], tmp_difficulty[34];
    unsigned char *g_buffer;
    unsigned int g_buffer_size = 0;
	
    unsigned char g_bestHash[34];
	unsigned long g_bestHashNonce = 0;
	unsigned long g_hashesTotal = 0;
	unsigned char g_debug = 1;
    int g_client = -1, g_server = -1;

	clock_type::time_point g_tstart;

	argon2_type type = Argon2_d;

    char arr[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    
	FILE *LogStream;
    
	void signal_handler(int signal)
	{
		//Logger("Got restart client signal?!\n");
		gSignalStatus = signal;
		program_done = true;
		data_loaded = true;
		new_data.notify_all();
		close(g_client);
        close(g_server);
	}

	inline std::string getCurrentDateTime(std::string s)
	{
		time_t now = time(0);
		struct tm tstruct;
		char buf[80];
		tstruct = *localtime(&now);
		if (s == "now")
			strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
		else if (s == "date")
			strftime(buf, sizeof(buf), "%Y-%m-%d", &tstruct);
		return std::string(buf);
	};

	inline void Logger(const char *fmt, ...)
	{
		va_list ap;
		time_t now = time(0);
		struct tm tstruct;
		char buf[80];
		tstruct = *localtime(&now);
		strftime(buf, sizeof(buf), "%X", &tstruct);
		fprintf(LogStream, "[%s]\t", buf);
		va_start(ap, fmt);
		vfprintf(LogStream, fmt, ap);
		fflush(LogStream);
		va_end(ap);
	}

    static inline uint32_t load32(const void *src)
    {
    #if defined(NATIVE_LITTLE_ENDIAN)
        return *(const uint32_t *)src;
    #else
        const uint8_t *p = (const uint8_t *)src;
        uint32_t w = *p++;
        w |= (uint32_t)(*p++) << 8;
        w |= (uint32_t)(*p++) << 16;
        w |= (uint32_t)(*p++) << 24;
        return w;
    #endif
    }

    void send_response(char *response, size_t size) {
        output_mutex.lock();
        size_t nwritten = send(g_client, response, size, 0);
        if (nwritten != size) 
            std::cout << "Error on sending nonce " << *response;
        output_mutex.unlock();
    }

	void run_miner(int threadID)
	{
		clock_type::time_point tstop;
		bool solution;
		unsigned char out[32];

		uint32_t length, pwdlen;
		unsigned char pwd[1024 * 1027 + 5];

		unsigned char target[33], bestHash[33];
		unsigned long i, j, k;

		unsigned long start = 0;
		unsigned long end = 0;
		unsigned long bestHashNonce;

		unsigned char hash[65];

		unsigned long idPrev = 0;

        argon2_context context;
        int result;

        context.out = (uint8_t *)out;
        context.outlen = (uint32_t)32;
        context.salt = CONST_CAST(uint8_t *)"Satoshi_is_Finney";
        context.saltlen = (uint32_t)17;
        context.secret = NULL;
        context.secretlen = 0;
        context.ad = NULL;
        context.adlen = 0;
        context.t_cost = 2;
        context.m_cost = 256;
        context.lanes = 2;
        context.threads = 1;
        context.allocate_cbk = NULL;
        context.free_cbk = NULL;
        context.flags = ARGON2_DEFAULT_FLAGS;
        context.version = ARGON2_VERSION_13;
        context.pwd = CONST_CAST(uint8_t *)pwd;

		while (!program_done)
		{
			start = 0;
            end = 0;

			input_mutex.lock();
			if (g_length > 0 && g_end > 0 && g_start < g_end)
			{
				start = g_start;
				end = g_start + 500;
				if (end > g_end)
					end = g_end;

				g_start = end;
                g_working++;

				if (idPrev != g_id)
				{
					length = (int32_t)g_length;
					memcpy(pwd, g_pwd, length);
                    
                    pwd[length + 4] = 0;
                    pwd[length + 5] = 0;

					memcpy(target, g_difficulty, 32);
					memset(bestHash, 0xff, 32);

					context.pwdlen = (uint32_t)(length + 4);
					idPrev = g_id;
				}
                if (g_debug)
                    Logger("%d New Work Batch! %lu %lu\n", threadID, start, end);
			}
			input_mutex.unlock();

			if (end == 0) {
				// there's no new data to process
				data_loaded = false;
				std::unique_lock<std::mutex> mlock(data_mutex);
				new_data.wait(mlock, [] {return data_loaded || program_done; });
				continue;
			}

			if (g_debug)
			    Logger("%d processing %lu %lu\n", threadID, start, end);

			j = start;
			solution = false;

			while ((j < end) && (solution == false))
			{
                pwd[length + 3] = j & 0xff;
                pwd[length + 2] = j >> 8 & 0xff;
                pwd[length + 1] = j >> 16 & 0xff;
                pwd[length    ] = j >> 24 & 0xff;
                result = argon2_ctx(&context, type);

                if (result != ARGON2_OK) {
                    Logger("Error Argon2d");
                    return;
                }

                for (i = 0; i < 32; i++)
                {
                    if (bestHash[i] == out[i])
                        continue;
                    else if (bestHash[i] < out[i])
                        break;
                    else if (bestHash[i] > out[i])
                    {
                        memcpy(bestHash, out, 32);
                        bestHashNonce = j;
                        for (i = 0; i < 32; i++)
                        {
                            if (target[i] == bestHash[i])
                                continue;
                            else if (target[i] < bestHash[i])
                                break;
                            else if (target[i] > bestHash[i])
                            {
                                input_mutex.lock();
                                if (idPrev == g_id) {
                                    g_start = g_end + 1;
                                    solution = true;
                                } else  //it was changed already
                                    j = end;
                                input_mutex.unlock();
                                solution = true;
                                break;
                            }
                        }
                        break;
                    }
                }
                j++;
            }

			if (g_debug)
				Logger("processing ENDED %lu %lu initially %lu %lu\n", start, end, g_start, g_end);

            
            input_mutex.lock();
            g_working--;

            if (g_id == idPrev)
            {
                g_hashesTotal += (j - start);

                for (i = 0; i < 32; i++)
                {
                    if (g_bestHash[i] == bestHash[i])
                        continue;
                    else if (g_bestHash[i] < bestHash[i])
                        break;
                    else if (g_bestHash[i] > bestHash[i])
                    {
                        memcpy(g_bestHash, bestHash, 32);
                        g_bestHashNonce = bestHashNonce;
                        break;
                    }
                }

                if (g_debug)
                    Logger("processing ENDED %lu %lu initially %lu %lu\n", start, end, g_start, g_end);

                if ( (solution == true) ||
                    (g_working == 0 && g_start == g_end && g_end != 0))
                {           
                    for (i = 0; i < 32; i++)
                    {
                        hash[2 * i    ] = arr[g_bestHash[i] / 16];
                        hash[2 * i + 1] = arr[g_bestHash[i] % 16];
                    }
                    hash[64] = 0;

                    tstop = clock_type::now();
                    clock_type::duration compute_time = tstop - g_tstart;
                    double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(compute_time).count();

                    if (g_debug)
                    {
                        Logger("Time elapsed in s: %f\n", elapsed / 1000);
                        Logger("Total H/s: %f\n", (g_hashesTotal / elapsed * 1000));
                    }

                    char output[2048];
                    if (solution == true)
                        sprintf(output, "{ \"type\": \"s\", \"hash\": \"%s\", \"nonce\": %lu , \"h\": %lu }", hash, g_bestHashNonce, (unsigned long)(g_hashesTotal / elapsed * 1000));
                    else
                        sprintf(output, "{ \"type\": \"b\", \"bestHash\": \"%s\", \"bestNonce\": %lu , \"h\": %lu }", hash, g_bestHashNonce, (unsigned long)(g_hashesTotal / elapsed * 1000));
                    send_response(output, strlen(output));
                    if (g_debug) Logger("%d Data delivered: start=%lu end=%lu gstart=%lu gend=%lu\n", threadID, start, end, g_start, g_end);
                }
            }
            input_mutex.unlock();
        }
        if (g_debug)
            Logger("%d Thread Done!\n", threadID);
	}

	int interpretData(unsigned char *data)
	{
		unsigned long i;		
		unsigned long _start, _length;
        bool ok;
        
		std::lock_guard<std::mutex> guard(data_mutex);
		input_mutex.lock();

        _start = load32(data);data += 4;
        _length = load32(data);data += 4;

        if (_start == 0 && _length == 0) {
            program_done = true;
            close(g_client);
            close(g_server);
            return -1;
        }

        memcpy(tmp_pwd, data, _length);
        data += _length;
        memcpy(tmp_difficulty, data, 32);
        data += 32;
        g_end = load32(data);

        if (_start == g_startPrev && _length == g_length) {
            ok = false;
            for (i=0; i < _length; i++)
                if (tmp_pwd[i] != g_pwd[i]){
                    ok = true;
                    break;
                }

            for (i=0; i < 32; i++)
                if (tmp_difficulty[i] != g_difficulty[i]){
                    ok = true;
                    break;
                }

            if (ok == false) {
                
			    input_mutex.unlock();
                return 0;
            }
        }
        memcpy(g_pwd, tmp_pwd, _length);
        memcpy(g_difficulty, tmp_difficulty, 32);

		g_tstart = clock_type::now();
		g_start = _start;
		g_startPrev = g_start;
		g_length = _length;
		memset(g_bestHash, 0xff, 32);
		g_bestHashNonce = 0;
		g_hashesTotal = 0;
		g_id++;
		input_mutex.unlock();
		data_loaded = true;
		new_data.notify_all();
		return 1;
	}
} // namespace

void handle_request(int client) {
    int nread = 0;

    while (!program_done) {
        uint32_t size = 0;
        nread = recv(client, &size, 4, 0);
        if (nread < 0) {
            if (errno == EINTR) {
                // the socket call was interrupted -- try again
                std::cout << "the socket call was interrupted -- try again" <<std::endl;
                continue;
            }
            else {

                // an error occurred, so break out
                std::cout << "an error occurred, so break out" <<std::endl;
                program_done = true;
                close(g_server);
                return;
            }
        } else if (nread == 0) {
            // the socket is closed
            std::cout << "the socket is closed" << std::endl;
            program_done = true;
            close(g_server);
            return;
        }

        // the rest
        if (size > g_buffer_size) {
            if (g_buffer_size != 0) free(g_buffer);
            g_buffer = (unsigned char *)malloc(size);
            g_buffer_size = size;
        }

        memset(g_buffer, 0, size);

        nread = recv(client, g_buffer, size, 0);
        if (nread < 0) {
            if (errno == EINTR) {
                // the socket call was interrupted -- try again
                std::cout << "the socket call was interrupted -- try again" <<std::endl;
                continue;
            }
            else {
                // an error occurred, so break out
                std::cout << "an error occurred, so break out" <<std::endl;
                program_done = true;
                close(g_server);
                return;
            }
        } else if (nread == 0) {
            // the socket is closed
            std::cout << "the socket is closed" << std::endl;
            program_done = true;
            close(g_server);
            return;
        }

        if (nread != size) {
            std::cout << "there's more to read..." << std::endl;
            program_done = true;
            close(g_server);
        }

        interpretData(g_buffer);
    }
} // namespace

int main(int argc, char *argv[])
{
	int i;
	int nThreads = 0;
    int port = 3456;

	std::string filePath = "log_" + getCurrentDateTime("date") + ".txt";
	LogStream = fopen(filePath.c_str(), "w+");

	std::signal(SIGINT, signal_handler);

	//printf("PARSING \n");
	/* parse options */
	for (i = 0; i < argc; i++)
	{
		const char *a = argv[i];

		if (!strcmp(a, "-d"))
		{
			if (i < argc - 1)
			{
				i++;
				g_debug = atoi(argv[i]);
				continue;
			}
			else
			{
				printf("missing -d argument");
				return 0;
			}
		}
        else if (!strcmp(a, "-c")) {
            if (i < argc - 1) {
                i++;
                nThreads = atoi(argv[i]);
                continue;
            } else {
                printf("missing -c argument");
                return 0;
            }
        }
	}
    argon2_select_impl(stderr, "[libargon2] ");
    
	for (i = 0; i < nThreads; i++)
	{
        try
        {
            threads.push_back(std::thread(run_miner, i));
        }
        catch (std::exception &e)
        {
            std::cout << "Error creating thread " << i
                << std::endl
                << e.what() << std::endl;
        }
	}

    
    struct sockaddr_in server_addr,client_addr;
    socklen_t clientlen = sizeof(client_addr);
    int reuse;

    // setup socket address structure
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

      // create socket
    g_server = socket(PF_INET,SOCK_STREAM,0);
    if (!g_server) {
        perror("socket");
        exit(-1);
    }

      // set socket to immediately reuse port when the application closes
    reuse = 1;
    if (setsockopt(g_server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt");
        exit(-1);
    }

      // call bind to associate the socket with our local address and
      // port
    if (bind(g_server, (const struct sockaddr *)&server_addr,sizeof(server_addr)) < 0) {
        perror("bind");
        exit(-1);
    }

      // convert the socket to listen for incoming connections
    if (listen(g_server, SOMAXCONN) < 0) {
        perror("listen");
        exit(-1);
    }

    // accept clients
    if (((g_client = accept(g_server,(struct sockaddr *)&client_addr, &clientlen)) > 0) && (!program_done)) {
            std::cout << "New client!" << std::endl;

            // loop to handle all requests
            while (!program_done) {
                handle_request(g_client);
            }

            close(g_client);
    }

    std::cout << "Cleaning up, please wait!" << std::endl;

    for (auto &t : threads)
        t.join();

    free(g_buffer);
    fclose(LogStream);
    return 0;
}
