# Copyright © 2016 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifdef V
$(info Compiling application)
endif

# Adding path for jerry script APIs
DEPS_BASE ?= $(ZJS_BASE)/deps
ZEPHYRINCLUDE += -I$(JERRY_INCLUDE) -I$(DEPS_BASE)

obj-y += main-zephyr.o

obj-y += jerry-code.o

obj-y += file-wrapper.o
obj-y += ihex-handler.o

obj-y += acm-uart.o
obj-y += acm-shell.o
obj-y += shell-state.o

obj-y += ../../deps/ihex/kk_ihex_read.o