/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "Webapp.h"
#include "Session.h"
#include "Database.h"


using namespace std;
using namespace asio;
using namespace asio::ip;
using namespace std::placeholders;

#ifdef _WIN32
#pragma warning(disable:4316)
#endif

void Webapp::CreateWorker(const WorkerInit& init)
{
	workers.Add(init);
}

void Webapp::Start() {
	while(!aborted) {
		{
			WorkerInit init;
			Worker w(init);
			w.Cleanup();
		}
		
		workers.Start();

		workers.Clear();
		
		/* Workers have all aborted at this stage (single thread) */
		scripts.clear();
		databases.clear();
		leveldb_databases.clear();
	}
}

/**
 * Deconstruct Webapp object.
*/
Webapp::~Webapp()
{
	workers.Stop();
}
