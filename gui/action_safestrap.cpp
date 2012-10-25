// action_safestrap.cpp - GUIAction extension

int createImagePartition(string slotName, string imageName, int imageSize, string mountName, int loopNum, int progressBase1, int progressBase2, int progressBase3) {
	char cmd[512];


	DataManager::SetValue("tw_operation", "Clearing old " + imageName + ".img...");

	sprintf(cmd, "/%s", mountName.c_str());
	ensure_path_unmounted(cmd);
	sprintf(cmd, "rm -rf /ss/safestrap/%s/%s.img", slotName.c_str(), imageName.c_str());
	fprintf(stderr, "createImagePartition::%s\n", cmd);
	__system(cmd);

	sprintf(cmd, "rm /dev/block/%s", imageName.c_str());
	fprintf(stderr, "createImagePartition::%s\n", cmd);
	usleep(100000);
	__system(cmd);

	sprintf(cmd, "/sbin/bbx losetup -d /dev/block/loop%d", loopNum);
	fprintf(stderr, "createImagePartition::%s\n", cmd);
	usleep(100000);
	__system(cmd);
	DataManager::SetValue("ui_progress", progressBase1);

	DataManager::SetValue("tw_operation", "Creating " + imageName + ".img...");
	ui_print("Creating %s.img...\n", imageName.c_str());

	sprintf(cmd, "dd if=/dev/zero of=/ss/safestrap/%s/%s.img bs=1M count=%d", slotName.c_str(), imageName.c_str(), imageSize);
	fprintf(stderr, "createImagePartition::%s\n", cmd);
	usleep(100000);
	if (__system(cmd) != 0) return 1;
	sprintf(cmd, "/sbin/fsync /ss/safestrap/%s/%s.img", slotName.c_str(), imageName.c_str());
	fprintf(stderr, "createImagePartition::%s\n", cmd);
	usleep(100000);
	if (__system(cmd) != 0) return 1;
	DataManager::SetValue("ui_progress", progressBase2);

	DataManager::SetValue("tw_operation", "Writing filesystem on " + imageName + "...");
	ui_print("Writing filesystem on %s...\n", imageName.c_str());
	sprintf(cmd, "/sbin/bbx losetup /dev/block/loop%d /ss/safestrap/%s/%s.img", loopNum, slotName.c_str(), imageName.c_str());
	fprintf(stderr, "createImagePartition::%s\n", cmd);
	usleep(100000);
	if (__system(cmd) != 0) return 1;

	sprintf(cmd, "ln -s /dev/block/loop%d /dev/block/%s", loopNum, imageName.c_str());
	fprintf(stderr, "createImagePartition::%s\n", cmd);
	usleep(100000);
	if (__system(cmd) != 0) return 1;

#ifdef USE_EXT4
	sprintf(cmd, "mke4fs /dev/block/%s", imageName.c_str());
#else
	sprintf(cmd, "mke2fs /dev/block/%s", imageName.c_str());
#endif
	fprintf(stderr, "createImagePartition::%s", cmd);
	usleep(100000);
	if (__system(cmd) != 0) return 1;
	sprintf(cmd, "/sbin/fsync /ss/safestrap/%s/%s.img", slotName.c_str(), imageName.c_str());
	fprintf(stderr, "createImagePartition::%s\n", cmd);
	usleep(100000);
	if (__system(cmd) != 0) return 1;

#ifndef USE_EXT4
	sprintf(cmd, "tune2fs -j /dev/block/%s", imageName.c_str());
	fprintf(stderr, "createImagePartition::%s\n", cmd);
	usleep(100000);
	if (__system(cmd) != 0) return 1;
	sprintf(cmd, "/sbin/fsync /ss/safestrap/%s/%s.img", slotName.c_str(), imageName.c_str());
	fprintf(stderr, "createImagePartition::%s\n", cmd);
	usleep(100000);
	if (__system(cmd) != 0) return 1;
#endif
	DataManager::SetValue("ui_progress", progressBase3);

	return 0;
}

int GUIAction::doSafestrapAction(Action action, int isThreaded /* = 0 */) {
	int simulate;
	std::string arg = gui_parse_text(action.mArg);
	std::string function = gui_parse_text(action.mFunction);
	DataManager::GetValue(TW_SIMULATE_ACTIONS, simulate);

	LOGD("doSafestrapAction(%s(%s, %d));\n", function.c_str(), arg.c_str(), isThreaded);

	if (function == "refreshsizesnt")
	{
		if (simulate) {
			simulate_progress_bar();
		} else
			update_system_details();
		return 0;
	}

	if (function == "changeslot") {
		char cmd[512];
		ensure_path_unmounted("/system");
		ensure_path_unmounted("/data");
		// move cache/recovery
		ensure_path_unmounted("/cache");
		sprintf(cmd, "/sbin/changeslot.sh %s", arg.c_str());
		__system(cmd);
		ensure_path_mounted("/cache");
		getLocations();
		DataManager::SetValue("tw_bootslot", arg);
		return 0;
	}

	if (function == "readbootslot") {
		char array[512];
		long lSize;
		size_t result;
		string str = "stock";

		// Read in the file, if possible
		FILE* in = fopen("/ss/safestrap/active_slot", "rb");
		if (!in) return 0;

		// obtain file size:
		fseek(in, 0, SEEK_END);
		lSize = ftell(in);
		LOGD("lSize=%d\n", lSize);
		if (lSize > 0) {
			fseek(in, 0, SEEK_SET);
			// adjust for EOF
			result = fread(array, 1, lSize, in);
			LOGD("result=%d\n",(long)result);
			if ((long)result == lSize) {
				array[lSize-1] = '\0';
				str = array;
			}
		}
		fclose(in);
//		ui_print("bootslot set: %s\n", str.c_str());
		DataManager::SetValue(arg, str);
		return 0;
	}

	if (function == "checkslotnames") {
		char array[512];
		string str = "Undefined";
		string var = "";

		for (int i = 1; i < 5; i++) {
			sprintf(array, "rom-slot%d", i);
			var = array;
			DataManager::GetValue("tw_" + var + "_name", str);
			fprintf(stderr, "checkslotnames: tw_%s_name=%s\n", var.c_str(), str.c_str());
		}
		return 0;	
	}

	if (function == "readslotnames") {
		int i;
		char array[512];
		char filename[512];
		long lSize;
		size_t result;
		string str = "Undefined";
		string var = "";

		DataManager::SetValue("tw_rom-slot1_name", "ROM-Slot-1");
		DataManager::SetValue("tw_rom-slot2_name", "ROM-Slot-2");
		DataManager::SetValue("tw_rom-slot3_name", "ROM-Slot-3");
		DataManager::SetValue("tw_rom-slot4_name", "ROM-Slot-4");

		for (int i = 1; i < 5; i++) {
			sprintf(array, "rom-slot%d", i);
			var = array;

			// Read in the file, if possible
			sprintf(filename, "/ss/safestrap/rom-slot%d/name", i);
			FILE* in = fopen(filename, "rb");
			if (in) {
				// obtain file size:
				fseek(in, 0, SEEK_END);
				lSize = ftell(in);
				LOGD("lSize=%d\n", lSize);
				if (lSize > 0) {
					fseek(in, 0, SEEK_SET);
					// adjust for EOF
					result = fread(array, 1, lSize, in);
					LOGD("result=%d\n",(long)result);
					if ((long)result == lSize) {
						array[lSize-1] = '\0';
						str = array;
					}
				}
				fclose(in);
				fprintf(stderr, "found: tw_%s_name=%s\n", var.c_str(), str.c_str());
				DataManager::SetValue("tw_" + var + "_name", str);
			}
			else {
				DataManager::GetValue("tw_" + var + "_name", str);
				fprintf(stderr, "not found: tw_%s_name=%s\n", var.c_str(), str.c_str());
			}
		}
		return 0;
	}

	if (function == "setslotvarname") {
		char array[512];
		string str = arg;
		string var = "tw_" + arg +"_name";
		DataManager::GetValue(var, str);
		fprintf(stderr, "Loaded from %s, setting tw_slotname: %s\n", var.c_str(), str.c_str());
		DataManager::SetValue("tw_slotname", str);
		return 0;
	}

	if (function == "setslotnickname") {
		char cmd[512];
		string romslot;
		string slotname;

		DataManager::GetValue("tw_trybootslot", romslot);
		DataManager::GetValue("tw_try_slotname", slotname);
		if ((romslot == "stock") || (slotname == ""))
			return 0;

		fprintf(stderr, "setslotnickname: echo \"%s\" > /ss/safestrap/%s/name\n", slotname.c_str(), romslot.c_str());
		sprintf(cmd, "echo \"%s\" > /ss/safestrap/%s/name", slotname.c_str(), romslot.c_str());
		__system(cmd);

		DataManager::SetValue("tw_" + romslot + "_name", slotname);
		return 0;
	}

	return -1;
}

int GUIAction::doSafestrapThreadedAction(Action action, int isThreaded /* = 0 */) {
	int simulate;
	std::string pageName = gui_parse_text(action.mArg);
	std::string function = gui_parse_text(action.mFunction);
	std::string arg = "stock";
	DataManager::GetValue(TW_SIMULATE_ACTIONS, simulate);
	DataManager::GetValue("tw_trybootslot", arg);

	if (function == "createslot") {
		operation_start("createslot");
		if (arg == "stock")
			return 0;

		if (simulate) {
			simulate_progress_bar();
			ui_print("Simulating actions...\n");
		}
		else {
			int system_size = 600;
			int data_size = 500;
			int cache_size = 300;
			char cmd[512];

			gui_changePage(pageName);

			DataManager::GetValue("tw_slot_system_size", system_size);
			DataManager::GetValue("tw_slot_data_size", data_size);
			DataManager::GetValue("tw_slot_cache_size", cache_size);

			DataManager::SetValue("ui_progress", 0);

			sprintf(cmd, "mkdir -p /ss/safestrap/%s", arg.c_str());
			__system(cmd);

			// SYSTEM
			if (createImagePartition(arg, "system", system_size, "system", 7, 5, 20, 30) != 0) {
				fprintf(stderr, "Error creating system partition!");
				DataManager::SetValue("tw_operation", "Error creating system partition!");
				ui_print("Error creating system partition!\n");
				ui_print("Cleaning up files...\n");
				sprintf(cmd, "rm -rf /ss/safestrap/%s", arg.c_str());
				__system(cmd);
				return -1;
			}

			// USERDATA
			if (createImagePartition(arg, "userdata", data_size, "data", 6, 35, 60, 70) != 0) {
				DataManager::SetValue("tw_operation", "Error creating data partition!");
				ui_print("Error creating data partition!\n");
				ui_print("Cleaning up files...\n");
				sprintf(cmd, "rm -rf /ss/safestrap/%s", arg.c_str());
				__system(cmd);
				return -1;
			}

			// CACHE
			ensure_path_mounted("/cache");
			__system("cp -R /cache/recovery /tmp");
			__system("rm -rf /cache/recovery");
			ensure_path_unmounted("/cache");

			if (createImagePartition(arg, "cache", cache_size, "cache", 5, 75, 85, 90) != 0) {
				DataManager::SetValue("tw_operation", "Error creating cache partition!");
				ui_print("Error creating cache partition!\n");
				ui_print("Cleaning up files...\n");
				sprintf(cmd, "rm -rf /ss/safestrap/%s", arg.c_str());
				__system(cmd);
				ensure_path_mounted("/cache");
				__system("rm -rf /cache/recovery");
				__system("cp -R /tmp/recovery /cache");
				__system("rm -rf /tmp/recovery");
				return -1;
			}

			ensure_path_mounted("/cache");
			__system("rm -rf /cache/recovery");
			__system("cp -R /tmp/recovery /cache");
			__system("rm -rf /tmp/recovery");

			DataManager::SetValue("tw_operation", "Activating ROM slot...");
			ui_print("Activating ROM slot...\n");
			sprintf(cmd, "echo \"%s\" > /ss/safestrap/active_slot", arg.c_str());
			__system(cmd);
			DataManager::SetValue("ui_progress", 95);

			DataManager::SetValue("tw_operation", "Updating filesystem details...");
			ui_print("Updating filesystem details...\n");
			getLocations();
			DataManager::SetValue("ui_progress", 100);
			DataManager::SetValue("tw_bootslot", arg);

			// Done
			DataManager::SetValue("ui_progress", 0);
		}
		operation_end(0, simulate);
		return 0;
	}

	if (function == "deleteslot") {
		char cmd[512];

		operation_start("deleteslot");
		if (arg == "stock")
			return 0;

		if (simulate) {
			simulate_progress_bar();
			ui_print("Simulating actions...\n");
		}
		else {
			sprintf(cmd, "rm -rf /ss/safestrap/%s", arg.c_str());
			__system(cmd);
		}
		operation_end(0, simulate);
		return 0;
	}
	return -1;
}
