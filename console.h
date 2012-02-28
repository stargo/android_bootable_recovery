/*
 * Copyright (C) 2010 Skrilax_CZ
 * Open Recovery Console
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#ifndef CONSOLE_H_
#define CONSOLE_H_

#define CONSOLE_FORCE_QUIT 		-55
#define CONSOLE_FAILED_START 	-56

int run_console(const char* command);
void show_console_menu();

#endif //!CONSOLE_H_
