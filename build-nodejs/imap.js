/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2013 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

"use strict";

var net = require('net');
var tls = require('tls')
var etpan = require('./build/Release/etpan.node');
var imapsetModule = require('./imapset.js');
var starttls = require('starttls');

// TODO:
// idle: get result
//
// later:
// SASL
// namespace
// condstore
// qresync
// quota
// compress

var imapset = imapsetModule.imapset;

var mailimap = function() {
  // * private
  // _state
  // _currentCallback
  // _appendState
  // _queue
  // _idleState
  // _socket
  // _originalSocket
  // _currentTag
  
  this._init();
};

//module.exports = {};

function define(name, value) {
    Object.defineProperty(module.exports, name, {
        value:      value,
        enumerable: true
    });
    
    mailimap[name] = value;
}
// Public API

/*
 imap.append('Anna', 'From ...', mailimap.IMAPFlagSeen, new Date(), function(error, uids) {
   // do something
 });
*/
mailimap.prototype.append = function(folder, messageData, flags, date, callback) {
  this._append(folder, messageData, flags, date, callback);
};

/*
 imap.connect({'hostname':'imap.gmail.com', 'port':993, 'connectionType':'ssl'}, function(error, capabilities) {
   // do something
 });
*/
mailimap.prototype.connect = function(connectInfo, callback) {
  this._connect(connectInfo, callback);
};

/*
 imap.login({'username':'my-login', 'password':'mypassword'}, function(error) {
   // do something
 });

 imap.login({'username':'my-login', 'password':'mypassword', 'saslType':'crammd5'}, function(error) {
   // do something
 });

 imap.login({'username':'my-login', 'token':'theoauthtoken', 'saslType':'xoauth2'}, function(error) {
   // do something
 });
*/
mailimap.prototype.login = function(options, callback) {
  if (options.saslType == 'xoauth2') {
    this._oauth2Authenticate(options, callback);
  }
  else if (options.saslType != null) {
    this._authenticate(options, callback);
  }
  else {
    this._login(options, callback);
  }
};

/*
 imap.logout(function(error) {
   // do something
 });
*/
mailimap.prototype.logout = function(callback) {
  this._logout(callback);
};

/*
 // happens synchronously.

 imap.disconnect();
*/
mailimap.prototype.disconnect = function() {
  this._disconnect();
};

/*
 imap.noop(function(error) {
   // do something
 });
*/
mailimap.prototype.noop = function(callback) {
  this._noop(callback);
};

/*
 imap.capability(function(error, capabilities) {
   // do something
 });
*/
mailimap.prototype.capability = function(callback) {
  this._capability(callback);
};

/*
 imap.check(function(error) {
   // do something
 });
*/
mailimap.prototype.check = function(callback) {
  this._check(callback);
};

/*
 imap.close(function(error) {
   // do something
 });
*/
mailimap.prototype.close = function(callback) {
  this._close(callback);
};

/*
 imap.expunge(function(error) {
   // do something
 });
*/
mailimap.prototype.expunge = function(callback) {
  this._expunge(callback);
};

/*
 imap.copy([1453, 3456], 'Anna', {}, function(error, copyInfo) {
   // do something
 });

 imap.copy([1453, 3456], 'Anna', { 'byuid': true }, function(error, copyInfo) {
   // do something
 });
*/
mailimap.prototype.copy = function(uids, destFolder, options, callback) {
  this._copy(uids, destFolder, options, callback);
};

/*
 imap.create('Anna', function(error) {
   // do something
 });
*/
mailimap.prototype.create = function(folder, callback) {
  this._create(folder, callback);
};

/*
 imap.delete('Anna', function(error) {
   // do something
 });
*/
mailimap.prototype.delete = function(folder, callback) {
  this._delete(folder, callback);
};

/*
 imap.rename('Anna', 'Anna Emails', function(error) {
   // do something
 });
*/
mailimap.prototype.rename = function(folder, otherName, callback) {
  this._rename(folder, callback);
};

/*
 imap.examine('INBOX', function(error) {
   // do something
 });
*/
mailimap.prototype.examine = function(folder, callback) {
  this._examine(folder, callback);
};

/*
 fetchAtt = [{type:mailimap.FetchEnvelope}, {type:mailimap.FetchBodySection, param:'1.2'}];
 imap.fetch(new IndexSet(1, MAX_UID), fetchAtt, {}, function(error, messages) {
   // do something
 });

 imap.fetch(new IndexSet(1, MAX_UID), fetchAtt, { 'byuid': true }, function(error, messages) {
   // do something
 });
*/
mailimap.prototype.fetch = function(numbers, fetchAtt, options, callback) {
  this._fetch(numbers, fetchAtt, options, callback);
};

/*
 imap.lsub(prefix, pattern, function(error, folders) {
   // do something
 });
*/
mailimap.prototype.lsub = function(prefix, pattern, callback) {
  this._lsub(prefix, pattern, callback);
};

/*
 imap.list(prefix, pattern, function(error, folders) {
   // do something
 });
*/
mailimap.prototype.list = function(prefix, pattern, callback) {
  this._list(prefix, pattern, callback);
};

/*
 imap.search('foo', searchKey, { 'charset': 'utf-8' }, function(error) {
   // do something
 });

 imap.search('foo', searchKey, { 'byuid': true }, function(error) {
   // do something
 });
*/
mailimap.prototype.search = function(searchKey, options, callback) {
  this._search(searchKey, options, callback);
};


/*
 imap.select(folder, function(error, folderInfos) {
   // do something
 });
*/
mailimap.prototype.select = function(folder, callback) {
  this._select(folder, callback);
};

/*
 imap.status('INBOX', IMAPStatusSeen | IMAPStatusMessages, function(error, folderInfos) {
   // do something
 });
*/
mailimap.prototype.status = function(folder, callback) {
  this._status(folder, callback);
};

/*
 msgSet = new imapset();;
 msgSet.addIndex(1453);
 msgSet.addIndex(3456);
 imap.store(msgSet, {'flags':MessageFlagDeleted, 'type':StoreAdd, 'byuid': true }, function(error) {
   // do something
 });

 imap.store(msgSet, {'gmailLabels':['mylabel'], 'type':StoreAdd, 'byuid': true }, function(error) {
   // do something
 });
*/
mailimap.prototype.store = function(uids, options, callback) {
  this._store(uids, options, callback);
};

/*
 imap.subscribe('Anna', function(error) {
   // do something
 });
*/
mailimap.prototype.subscribe = function(folder, callback) {
  this._subscribe(folder, callback);
};

/*
 imap.unsubscribe('Anna', function(error) {
   // do something
 });
*/
mailimap.prototype.unsubscribe = function(folder, callback) {
  this._unsubscribe(folder, callback);
};

/*
 imap.starttls(function(error) {
   // do something
 });
*/
mailimap.prototype.starttls = function(callback) {
  this._starttls(callback);
};

/*
 imap.idle(function(error, foldersInfo) {
   // do something
 });
*/
mailimap.prototype.idle = function(callback) {
  this._idle(callback);
};

mailimap.prototype.idleDone = function() {
  this._idleDone();
}

/*
 imap.enable([CapabilityRresync], function(error, enableEnabled) {
   // do something
 });
*/
mailimap.prototype.enable = function(capabilities, callback) {
  this._enable(capabilities, callback);
}

/*
 imap.id({'name': 'libetpan', 'version': '1.2'}, function(error, foldersInfo) {
   // do something
 });
*/
mailimap.prototype.id = function(clientInfo, callback) {
  this._id(clientInfo, callback);
};

// Private - implementation

mailimap.DISCONNECTED = 0;
mailimap.CONNECTED = 1;
mailimap.LOGGEDIN = 2;
mailimap.SELECTED = 3;

mailimap.MAILIMAP_NO_ERROR = 0;
mailimap.MAILIMAP_NO_ERROR_AUTHENTICATED = 1;
mailimap.MAILIMAP_NO_ERROR_NON_AUTHENTICATED = 2;
mailimap.MAILIMAP_ERROR_BAD_STATE = 3;
mailimap.MAILIMAP_ERROR_STREAM = 4;
mailimap.MAILIMAP_ERROR_PARSE = 5;
mailimap.MAILIMAP_ERROR_CONNECTION_REFUSED = 6;
mailimap.MAILIMAP_ERROR_MEMORY = 7;
mailimap.MAILIMAP_ERROR_FATAL = 8;
mailimap.MAILIMAP_ERROR_PROTOCOL = 9;
mailimap.MAILIMAP_ERROR_DONT_ACCEPT_CONNECTION = 10;
mailimap.MAILIMAP_ERROR_APPEND = 11;
mailimap.MAILIMAP_ERROR_NOOP = 12;
mailimap.MAILIMAP_ERROR_LOGOUT = 13;
mailimap.MAILIMAP_ERROR_CAPABILITY = 14;
mailimap.MAILIMAP_ERROR_CHECK = 15;
mailimap.MAILIMAP_ERROR_CLOSE = 16;
mailimap.MAILIMAP_ERROR_EXPUNGE = 17;
mailimap.MAILIMAP_ERROR_COPY = 18;
mailimap.MAILIMAP_ERROR_UID_COPY = 19;
mailimap.MAILIMAP_ERROR_CREATE = 20;
mailimap.MAILIMAP_ERROR_DELETE = 21;
mailimap.MAILIMAP_ERROR_EXAMINE = 22;
mailimap.MAILIMAP_ERROR_FETCH = 23;
mailimap.MAILIMAP_ERROR_UID_FETCH = 24;
mailimap.MAILIMAP_ERROR_LIST = 25;
mailimap.MAILIMAP_ERROR_LOGIN = 26;
mailimap.MAILIMAP_ERROR_LSUB = 27;
mailimap.MAILIMAP_ERROR_RENAME = 28;
mailimap.MAILIMAP_ERROR_SEARCH = 29;
mailimap.MAILIMAP_ERROR_UID_SEARCH = 30;
mailimap.MAILIMAP_ERROR_SELECT = 31;
mailimap.MAILIMAP_ERROR_STATUS = 32;
mailimap.MAILIMAP_ERROR_STORE = 33;
mailimap.MAILIMAP_ERROR_UID_STORE = 34;
mailimap.MAILIMAP_ERROR_SUBSCRIBE = 35;
mailimap.MAILIMAP_ERROR_UNSUBSCRIBE = 36;
mailimap.MAILIMAP_ERROR_STARTTLS = 37;
mailimap.MAILIMAP_ERROR_INVAL = 38;
mailimap.MAILIMAP_ERROR_EXTENSION = 39;
mailimap.MAILIMAP_ERROR_SASL = 40;
mailimap.MAILIMAP_ERROR_SSL = 41;
mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA = 42;

mailimap.PARSER_ENABLE_RESPONSE = 1 << 0;
mailimap.PARSER_ENABLE_GREETING = 1 << 1;
mailimap.PARSER_CONT_REQ = 1 << 2;

mailimap.prototype._init = function() {
  this._currentCallback = null;
  this._queue = [];
  this._state = mailimap.DISCONNECTED;
  
  this._idleState = mailimap.IDLE_STATE_WAIT_NONE;
  this._originalSocket = null;
  this._currentTag = 1;
};

mailimap.prototype._closeSocket = function() {
  if (this._socket != null) {
    this._socket.removeAllListeners('data');
    this._socket.removeAllListeners('end');
    this._socket.removeAllListeners('error');
    this._socket.removeAllListeners('timeout');
    this._socket.destroy();
    this._socket = null;
  }
  this._state = mailimap.DISCONNECTED;
  
  if (this._originalSocket != null) {
    this._socket = this._originalSocket;
    this._originalSocket = null;
    this._closeSocket();
  }
};

mailimap.prototype._stopListening = function() {
  if (this._socket != null) {
    this._socket.removeAllListeners('data');
  }
};

mailimap.prototype._queueIfNeeded = function(f, args) {
  if (this._currentCallback == null) {
    console.log('not queued');
    return false;
  }
  
  this._queue.push({'f':f, 'args':args});
  console.log('queued');
  console.log(this._queue);
  return true;
};

mailimap.prototype._runQueue = function() {
  if (this._queue.length == 0)
    return;
  
  var f = this._queue[0].f;
  var args = this._queue[0].args;
  this._queue.shift();
  f.apply(this, args);
};

mailimap.prototype._runCallback = function() {
  this._stopListening();
  if (this._currentCallback != null) {
    var currentCallback = this._currentCallback;
    this._currentCallback = null;
    currentCallback.apply(null, arguments);
  }
  
  this._runQueue();
};

mailimap.prototype._closeSocketWithError = function(error) {
  console.log('close socket');
  this._closeSocket();
  
  this._runCallback(error);
}

mailimap.prototype._connect = function(connectInfo, callback) {
  if (this._queueIfNeeded(this._connect, arguments)) {
    return;
  }
  
  if (this._state != mailimap.DISCONNECTED) {
    var error = new Error('Already connected');
    error.type = 'state_error';
    callback(error);
    return
  }
  
  console.log(connectInfo);
  
  this._buffer = new Buffer(0);
  if ((connectInfo.connectionType == null) || (connectInfo.connectionType == mailimap.ConnectionClear)) {
    this._socket = net.connect({
      'port':connectInfo.port,
      'host':connectInfo.hostname,
    }, this._connected.bind(this, callback));
  }
  else if (connectInfo.connectionType == mailimap.ConnectionSSL) {
    this._socket = tls.connect({
      'port':connectInfo.port,
      'host':connectInfo.hostname,
    }, this._connected.bind(this, callback));
  }
  this._currentCallback = callback;

  this._socket.on('data', function(data) {
    console.log(data.toString());
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_GREETING);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      this._state = mailimap.CONNECTED;
      console.log('cap');
      var capabilities = r.getCapabilitiesFromResponse();
      console.log('cap: ' + capabilities);
      this._runCallback(null, capabilities);
    }
    else {
      var error = new Error('Stream error');
      error.type = 'stream_error';
      this._closeSocketWithError(error);
    }
  }.bind(this));
  
  this._socket.on('end', function() {
    var error = new Error('Connection closed');
    error.type = 'stream_error';
    this._closeSocketWithError(error);
  }.bind(this));
  
  this._socket.on('error', function() {
    var error = new Error('Stream error');
    error.type = 'stream_error';
    this._closeSocketWithError(error);
  }.bind(this));
  
  this._socket.on('timeout', function() {
    var error = new Error('Stream error');
    error.type = 'stream_error';
    this._closeSocketWithError(error);
  }.bind(this));
};

mailimap.prototype._connected = function(callback) {
  console.log('socket connected');
};

mailimap.prototype._login = function(options, callback) {
  if (this._queueIfNeeded(this._login, arguments)) {
    return;
  }
  
  if (this._state != mailimap.CONNECTED) {
    var error;
    if (this._state == mailimap.DISCONNECTED) {
      error = new Error('Not connected');
    }
    else  {
      error = new Error('Already logged in');
    }
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' login ' + options.username + ' ' + options.password + '\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      this._state = mailimap.LOGGEDIN;
      console.log('logged in');
      var capabilities = r.getCapabilitiesFromResponse();
      this._runCallback(null, capabilities);
    }
    else {
      var error = new Error('Authentication error');
      error.type = 'auth_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._oauth2Authenticate = function(options, callback) {
  if (this._queueIfNeeded(this._oauth2Authenticate, arguments)) {
    return;
  }
  
  if (this._state != mailimap.CONNECTED) {
    var error;
    if (this._state == mailimap.DISCONNECTED) {
      error = new Error('Not connected');
    }
    else  {
      error = new Error('Already logged in');
    }
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  var authenticationToken = new Buffer('user=' + options.username + '\u0001auth=Bearer ' + options.token + '\u0001\u0001').toString('base64');
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' authenticate XOAUTH2 ' + this.authenticationToken + '\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      this._state = mailimap.LOGGEDIN;
      console.log('logged in');
      this._runCallback(null);
    }
    else {
      var error = new Error('Authentication error');
      error.type = 'auth_error';
      this._runCallback(error);
    }
  }.bind(this));
};

// TODO: SASL
mailimap.prototype._authenticate = function(options, callback) {
  if (this._queueIfNeeded(this._authenticate, arguments)) {
    return;
  }
  
  if (this._state != mailimap.CONNECTED) {
    var error;
    if (this._state == mailimap.DISCONNECTED) {
      error = new Error('Not connected');
    }
    else  {
      error = new Error('Already logged in');
    }
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  //this._socket.write('01 authenticate XOAUTH2 ' + this.authenticationToken + '\r\n', 'utf8');
  this._currentCallback = callback;
  
  var error = new Error('Not implemented');
  error.type = 'auth_error';
  this._runCallback(error);
};

mailimap.prototype._select = function(folder, callback) {
  if (this._queueIfNeeded(this._select, arguments)) {
    return;
  }
  
  if ((this._state == mailimap.CONNECTED) || (this._state == mailimap.DISCONNECTED)) {
    var error;
    if (this._state == mailimap.DISCONNECTED) {
      error = new Error('Not connected');
    }
    else {
      error = new Error('Not logged in');
    }
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' select ' + this._toQuotedString(folder) + '\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('selected');
      this._state = mailimap.SELECTED;
      var info = r.getSelectResponseFromResponse();
      this._runCallback(null, info);
    }
    else {
      var error = new Error('select error');
      error.type = 'select_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._logout = function(callback) {
  if (this._queueIfNeeded(this._logout, arguments)) {
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' logout\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('logged out');
      this._closeSocketWithError(null);
    }
    else {
      var error = new Error('logout error');
      error.type = 'logout_error';
      this._runCallback(error);
    }
  }.bind(this));
}

mailimap.prototype._disconnect = function() {
  this._closeSocketWithError(null);
}

mailimap.prototype._noop = function(callback) {
  if (this._queueIfNeeded(this._noop, arguments)) {
    return;
  }
  
  if (this._state == mailimap.DISCONNECTED) {
    var error = new Error('Not connected');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' noop\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('noop done');
      var info = r.getNoopResponseFromResponse();
      this._runCallback(null, info);
    }
    else {
      var error = new Error('select error');
      error.type = 'auth_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._lsub = function(prefix, pattern, callback) {
  if (this._queueIfNeeded(this._lsub, arguments)) {
    return;
  }

  if ((this._state == mailimap.DISCONNECTED) || (this._state == mailimap.CONNECTED)) {
    var error = new Error('Not logged in');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' lsub ' + this._toQuotedString(prefix) + ' ' + this._toQuotedString(pattern) + '\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;

  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    console.log(r);
    
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
      console.log('needs more data?');
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('parsed');
      var folders = r.getFoldersFromResponseLsub();
      this._runCallback(null, folders);
    }
    else {
      var error = new Error('Fetch folders error');
      error.type = 'list_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._list = function(prefix, pattern, callback) {
  if (this._queueIfNeeded(this._list, arguments)) {
    return;
  }

  if ((this._state == mailimap.DISCONNECTED) || (this._state == mailimap.CONNECTED)) {
    var error = new Error('Not logged in');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' list ' + this._toQuotedString(prefix) + ' ' + this._toQuotedString(pattern) + '\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;

  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    console.log(this._buffer.toString());
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    console.log(r);
    
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
      console.log('needs more data?');
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('parsed');
      var folders = r.getFoldersFromResponseList();
      this._runCallback(null, folders);
    }
    else {
      var error = new Error('Fetch folders error');
      error.type = 'list_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.APPEND_STATE_WAITING_CONT_REQ = 0;
mailimap.APPEND_STATE_WAITING_RESPONSE = 1;

mailimap.prototype._append = function(prefix, pattern, callback) {
  if (this._queueIfNeeded(this._append, arguments)) {
    return;
  }

  if ((this._state == mailimap.DISCONNECTED) || (this._state == mailimap.CONNECTED)) {
    var error = new Error('Not logged in');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._appendState = mailimap.APPEND_STATE_WAITING_CONT_REQ;
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' append ' + this._toQuotedString(folder) + ' (\Seen) {' + messageData.length + '}\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    
    if (this._appendState == mailimap.APPEND_STATE_WAITING_CONT_REQ) {
      console.log('append: cont req');
      var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE | mailimap.PARSER_CONT_REQ);
      console.log(r);
      
      if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
        // Wait for more data.
        console.log('needs more data?');
      }
      else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
        if (r.type == mailimap.PARSER_CONT_REQ) {
          // ok to append the message
          this._buffer = new Buffer(0);
          this._socket.write(messageData);
          this._socket.write('\r\n');
        }
        else if (r.type == mailimap.PARSER_ENABLE_RESPONSE) {
          var error = new Error('append error');
          error.type = 'append_error';
          this._runCallback(error);
        }
      }
      else {
        var error = new Error('Data probably too large');
        error.type = 'append_error';
        this._runCallback(error);
      }
    }
    else if (this._appendState == mailimap.APPEND_STATE_WAITING_RESPONSE) {
      console.log('append: wait for response');
      var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
      console.log(r);
      
      if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
        // Wait for more data.
        console.log('needs more data?');
      }
      else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
        var uids = r.getUIDPlusAppendResponseFromResponse();
        this._runCallback(null, uids);
      }
      else {
        var error = new Error('Append error');
        error.type = 'append_error';
        this._runCallback(error);
      }
    }
  }.bind(this));
}

mailimap.prototype._fetchTypeToString = function(fetchType) {
  if (typeof fetchType == 'number') {
    switch (fetchType) {
    case mailimap.FetchTypeEnvelope:
      return "ENVELOPE";
    case mailimap.FetchTypeFlags:
      return "FLAGS";
    case mailimap.FetchTypeInternalDate:
      return "INTERNALDATE";
    case mailimap.FetchTypeRFC822:
      return "RFC822";
    case mailimap.FetchTypeRFC822Header:
      return "RFC822.HEADER";
    case mailimap.FetchTypeRFC822Size:
      return "RFC822.SIZE";
    case mailimap.FetchTypeRFC822Text:
      return "RFC822.TEXT";
    case mailimap.FetchTypeBody:
      return "BODY";
    case mailimap.FetchTypeBodyStructure:
      return "BODYSTRUCTURE";
    case mailimap.FetchTypeUID:
      return "UID";
    case mailimap.FetchTypeModSeq:
      return "MODSEQ";
    case mailimap.FetchTypeGmailThreadID:
      return "X-GM-THRID";
    case mailimap.FetchTypeGmailMessageID:
      return "X-GM-MSGID";
    case mailimap.FetchTypeGmailLabels:
      return "X-GM-LABELS";
    }
  }
  else {
    switch (fetchType.type) {
    case mailimap.FetchTypeBodySection:
      return "BODY[" + fetchType.section + "]";
    case mailimap.FetchTypeBodyPeekSection:
      return "BODY.PEEK[" + fetchType.section + "]";
    default:
      return this._fetchTypeToString(fetchType.type);
    }
  }
}

mailimap.prototype._fetchAttToString = function(fetchAtt) {
  if (fetchAtt instanceof Array) {
    var fetchString = '';
    fetchAtt.forEach(function(item, index) {
      if (fetchString != '') {
        fetchString += ' ';
      }
      //console.log('fetchatt: ' + item);
      fetchString += this._fetchTypeToString(item);
    }.bind(this));
    return fetchString;
  }
  else if (typeof fetchAtt == 'number') {
    switch (fetchAtt) {
    case mailimap.FetchTypeAll:
      return "ALL";
    case mailimap.FetchTypeFull:
      return "FULL";
    case mailimap.FetchTypeFast:
      return "FAST";
    default:
      return this._fetchTypeToString(fetchAtt);
    }
  }
  else { // is object
    return this._fetchTypeToString(fetchAtt);
  }
};

mailimap.prototype._indexSetToString = function(imapset) {
  var imapSetString = '';
  imapset.forEachRange(function(left, right) {
    if (imapSetString != '') {
      imapSetString += ',';
    }
    if (left == 0) {
      imapSetString += '*';
    }
    else {
      imapSetString += left;
    }
    
    if (left != right) {
      if (right == 0) {
        imapSetString += ':*';
      }
      else {
        imapSetString += ':' + right;
      }
    }
  });
  return imapSetString;
}

mailimap.prototype._fetch = function(numbers, fetchAtt, options, callback) {
  if (this._queueIfNeeded(this._fetch, arguments)) {
    return;
  }
  
  if (this._state != mailimap.SELECTED) {
    var error = new Error('Not selected');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  if (options.byuid) {
    this._socket.write(this._currentTag + ' uid fetch ' + this._indexSetToString(numbers) + ' (' + this._fetchAttToString(fetchAtt) + ')\r\n', 'utf8');
  }
  else {
    this._socket.write(this._currentTag + ' fetch ' + this._indexSetToString(numbers) + ' (' + this._fetchAttToString(fetchAtt) + ')\r\n', 'utf8');
  }
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    //console.log(data.toString());
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('fetch done: ' + this._currentCallback);
      var items = r.getFetchItemsFromResponse();
      //console.log(items);
      this._runCallback(null, items);
    }
    else {
      var error = new Error('fetch error');
      error.type = 'fetch_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._capability = function(callback) {
  if (this._queueIfNeeded(this._capability, arguments)) {
    return;
  }
  
  if (this._state == mailimap.DISCONNECTED) {
    var error = new Error('Not connected');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' capability\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('capabilities done: ' + this._currentCallback);
      var capabilities = r.getCapabilitiesFromResponse();
      this._runCallback(null, capabilities);
    }
    else {
      var error = new Error('capabilities error');
      error.type = 'capability_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._check = function(callback) {
  if (this._queueIfNeeded(this._check, arguments)) {
    return;
  }
  
  if (this._state != mailimap.SELECTED) {
    var error = new Error('No folder selected');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' check\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('check done: ' + this._currentCallback);
      this._runCallback(null);
    }
    else {
      var error = new Error('check error');
      error.type = 'check_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._close = function(callback) {
  if (this._queueIfNeeded(this._close, arguments)) {
    return;
  }
  
  if (this._state != mailimap.SELECTED) {
    var error = new Error('No folder selected');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' close\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('close done: ' + this._currentCallback);
      this._runCallback(null);
    }
    else {
      var error = new Error('close error');
      error.type = 'close_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._expunge = function(callback) {
  if (this._queueIfNeeded(this._expunge, arguments)) {
    return;
  }
  
  if (this._state != mailimap.SELECTED) {
    var error = new Error('No folder selected');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' expunge\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('expunge done: ' + this._currentCallback);
      this._runCallback(null);
    }
    else {
      var error = new Error('expunge error');
      error.type = 'expunge_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._copy = function(uids, destFolder, options, callback) {
  if (this._queueIfNeeded(this._copy, arguments)) {
    return;
  }
  
  if (this._state != mailimap.SELECTED) {
    var error = new Error('No folder selected');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  if (options.byuid) {
    this._socket.write(this._currentTag + ' uid copy ' + this._indexSetToString(uids) + ' ' + this._toQuotedString(destFolder) + '\r\n', 'utf8');
  }
  else {
    this._socket.write(this._currentTag + ' copy ' + this._indexSetToString(uids) + ' ' + this._toQuotedString(destFolder) + '\r\n', 'utf8');
  }
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('copy done: ' + this._currentCallback);
      var copyResponse = getUIDPlusCopyResponseFromResponse(r);
      var uids  = new imapset();
      uids.setRanges(copyResponse.sourceUids);
      copyResponse.sourceUids = uids;
      uids  = new imapset();
      uids.setRanges(copyResponse.destUids);
      copyResponse.destUids = uids;
      this._runCallback(null, copyResponse);
    }
    else {
      var error = new Error('copy error');
      error.type = 'copy_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._create = function(folder, callback) {
  if (this._queueIfNeeded(this._create, arguments)) {
    return;
  }
  
  if ((this._state == mailimap.DISCONNECTED) || (this._state == mailimap.CONNECTED)) {
    var error = new Error('Not logged in');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' create ' + this._toQuotedString(folder) + '\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('create done: ' + this._currentCallback);
      this._runCallback(null);
    }
    else {
      var error = new Error('create error');
      error.type = 'create_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._delete = function(folder, callback) {
  if (this._queueIfNeeded(this._delete, arguments)) {
    return;
  }
  
  if ((this._state == mailimap.DISCONNECTED) || (this._state == mailimap.CONNECTED)) {
    var error = new Error('Not logged in');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' delete ' + this._toQuotedString(folder) + '\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('delete done: ' + this._currentCallback);
      this._runCallback(null);
    }
    else {
      var error = new Error('delete error');
      error.type = 'delete_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._rename = function(folder, otherName, callback) {
  if (this._queueIfNeeded(this._rename, arguments)) {
    return;
  }
  
  if ((this._state == mailimap.DISCONNECTED) || (this._state == mailimap.CONNECTED)) {
    var error = new Error('Not logged in');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' rename ' + this._toQuotedString(folder) + ' ' + this._toQuotedString(otherName) + '\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('rename done: ' + this._currentCallback);
      this._runCallback(null);
    }
    else {
      var error = new Error('rename error');
      error.type = 'rename_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._examine = function(folder, callback) {
  if (this._queueIfNeeded(this._examine, arguments)) {
    return;
  }
  
  if ((this._state == mailimap.CONNECTED) || (this._state == mailimap.DISCONNECTED)) {
    var error;
    if (this._state == mailimap.DISCONNECTED) {
      error = new Error('Not connected');
    }
    else {
      error = new Error('Not logged in');
    }
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' examine ' + this._toQuotedString(folder) + '\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('selected');
      this._state = mailimap.SELECTED;
      var info = r.getSelectResponseFromResponse();
      this._runCallback(null, info);
    }
    else {
      var error = new Error('examine error');
      error.type = 'select_error';
      this._runCallback(error);
    }
  }.bind(this));
};

// ALL
// ANSWERED
// BCC string
// DELETED
// DRAFT
// FLAGGED
// FROM string
// HEADER field-name string
// KEYWORD flag
// LARGER n
// NEW
// NOT search-key
// OLD
// ON date
// OR search-key1 search-key2
// RECENT
// SEEN
// SENTBEFORE date
// SENTON date
// SENTSINCE date
// SINCE date
// SMALLER n
// SUBJECT string
// TEXT string
// TO string
// UID sequence-set
// UNANSWERED
// UNDELETED
// UNDRAFT
// UNFLAGGED
// UNKEYWORD flag
// UNSEEN

mailimap.prototype._singleSearchKeyToString = function(searchKey) {
  switch (searchKey.type) {
    case 'sequence': {
      return this._indexSetToString(searchKey.value);
    }
    case 'all': {
      return 'ALL';
    }
    case 'answered': {
      return 'ANSWERED';
    }
    case 'bcc': {
      return 'BCC ' + this._toQuotedString(searchKey.value);
    }
    case 'deleted': {
      return 'DELETED';
    }
    case 'draft': {
      return 'DRAFT';
    }
    case 'flagged': {
      return 'FLAGGED';
    }
    case 'from': {
      return 'FROM ' + this._toQuotedString(searchKey.value);
    }
    case 'header': {
      return 'HEADER ' + this._toQuotedString(searchKey.header) + ' ' + this._toQuotedString(searchKey.value);
    }
    case 'keyword': {
      return 'KEYWORD ' + searchKey.value;
    }
    case 'larger': {
      return 'LARGER ' + searchKey.value;
    }
    case 'new': {
      return 'NEW';
    }
    case 'not': {
      return 'NOT ' + this._searchKeyToString(searchKey.value, true);
    }
    case 'old': {
      return 'OLD';
    }
    case 'on': {
      return 'ON ' + this._dateToString(searchKey.value)
    }
    case 'or': {
      return 'OR ' + this._searchKeyToString(searchKey.left, true) + ' ' + this._searchKeyToString(searchKey.right, true);
    }
    case 'recent': {
      return 'RECENT';
    }
    case 'seen': {
      return 'SEEN';
    }
    case 'sentbefore': {
      return 'SENTBEFORE ' + this._dateToString(searchKey.value)
    }
    case 'senton': {
      return 'SENTON ' + this._dateToString(searchKey.value)
    }
    case 'sentsince': {
      return 'SENTSINCE ' + this._dateToString(searchKey.value)
    }
    case 'since': {
      return 'SINCE ' + this._dateToString(searchKey.value)
    }
    case 'smaller': {
      return 'SMALLER ' + this._dateToString(searchKey.value)
    }
    case 'subject': {
      return 'SUBJECT ' + this._toQuotedString(searchKey.value);
    }
    case 'text': {
      return 'TEXT ' + this._toQuotedString(searchKey.value);
    }
    case 'to': {
      return 'TO ' + this._toQuotedString(searchKey.value);
    }
    case 'uid': {
      return 'UID ' + this._indexSetToString(searchKey.value);
    }
    case 'unanswered': {
      return 'UNANSWERED';
    }
    case 'undeleted': {
      return 'UNDELETED';
    }
    case 'undraft': {
      return 'UNDRAFT';
    }
    case 'unseen': {
      return 'UNSEEN';
    }
  }
};

var monthStrArray = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];

mailimap.prototype._dateToString = function(date) {
  var monthStr = monthStrArray[date.getMonth()];
  return date.getDate() + '-' + monthStr + '-' + date.getFullYear();
};

mailimap.prototype._searchKeyToString = function(searchKey, needsParenthesis) {
  if (searchKey instanceof Array) {
    var result = '';
    var first = true;
    if (needsParenthesis) {
      result += '(';
    }
    searchKey.forEach(function(item) {
      if (first) {
        first = false;
      }
      else {
        result += ' ';
      }
      result += this._searchKeyToString(item);
    });
    if (needsParenthesis) {
      result += ')';
    }
  }
  else {
    return this._singleSearchKeyToString(searchKey);
  }
};

mailimap.prototype._search = function(searchKey, options, callback) {
  if (this._queueIfNeeded(this._search, arguments)) {
    return;
  }
  
  if (this._state != mailimap.SELECTED) {
    var error = new Error('Not logged in');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  var searchParam = '';
  if (options.charset != null) {
    searchParam += 'CHARSET ' + this._toQuotedString(options.charset);
  }
  if (searchParam.length != 0) {
    searchParam += ' ';
  }
  searchParam += this._searchKeyToString(searchKey, true);
  
  this._buffer = new Buffer(0);
  if (options.byuid) {
    this._socket.write(this._currentTag + ' uid search ' + searchParam + '\r\n', 'utf8');
  }
  else {
    this._socket.write(this._currentTag + ' search ' + searchParam + '\r\n', 'utf8');
  }
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    //console.log(data.toString());
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      var items = r.getSearchResponseFromResponse();
      this._runCallback(null, items);
    }
    else {
      var error = new Error('search error');
      error.type = 'search_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._status = function(folder, callback) {
  if (this._queueIfNeeded(this._status, arguments)) {
    return;
  }
  
  if ((this._state == mailimap.CONNECTED) || (this._state == mailimap.DISCONNECTED)) {
    var error;
    if (this._state == mailimap.DISCONNECTED) {
      error = new Error('Not connected');
    }
    else {
      error = new Error('Not logged in');
    }
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' status ' + this._toQuotedString(folder) + ' (messages recent uidnext uidvalidity unseen highestmodseq)\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    console.log(data.toString());
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      console.log('status');
      var statusInfo = r.getStatusResponseFromResponse();
      this._runCallback(null, statusInfo);
    }
    else {
      var error = new Error('status error');
      error.type = 'status_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._flagsToString = function(flags) {
  var result = '';
  
  if ((flags & mailimap.MessageFlagSeen) != 0) {
    result += ' \\Seen';
  }
  if ((flags & mailimap.MessageFlagAnswered) != 0) {
    result += ' \\Answered';
  }
  if ((flags & mailimap.MessageFlagFlagged) != 0) {
    result += ' \\Flagged';
  }
  if ((flags & mailimap.MessageFlagDeleted) != 0) {
    result += ' \\Deleted';
  }
  if ((flags & mailimap.MessageFlagDraft) != 0) {
    result += ' \\Draft';
  }
  if ((flags & mailimap.MessageFlagMDNSent) != 0) {
    result += ' $MDNSent';
  }
  if ((flags & mailimap.MessageFlagForwarded) != 0) {
    result += ' $Forwarded';
  }
  if ((flags & mailimap.MessageFlagSubmitPending) != 0) {
    result += ' $SubmitPending';
  }
  if ((flags & mailimap.MessageFlagSubmitted) != 0) {
    result += ' $Submitted';
  }
  
  return result.substr(1);
};

mailimap.prototype._labelsToString = function(labels) {
  var result = '';
  if (fetchAtt instanceof Array) {
    labels.forEach(function(item) {
      if (result.length != 0) {
        result += ' ';
      }
      result += this._toQuotedString(item);
    });
  }
  else {
  }
  result += this._toQuotedString(labels);
  return labels;
};

mailimap.prototype._store = function(uids, options, callback) {
  if (this._queueIfNeeded(this._store, arguments)) {
    return;
  }
  
  if (this._state != mailimap.SELECTED) {
    var error = new Error('Not logged in');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  var paramString = '';
  if (options.flags != null) {
    if (options.type == mailimap.StoreAdd) {
      paramString = '+FLAGS.SILENT';
    }
    else if (options.type == mailimap.StoreSet) {
      paramString = 'FLAGS.SILENT';
    }
    else if (options.type == mailimap.StoreRemove) {
      paramString = '-FLAGS.SILENT';
    }
    paramString += ' (' + this._flagsToString(options.flags) + ')';
  }
  else if (options.gmailLabels != null) {
    if (options.type == mailimap.StoreAdd) {
      paramString = '+X-GM-LABELS.SILENT';
    }
    else if (options.type == mailimap.StoreSet) {
      paramString = 'X-GM-LABELS.SILENT';
    }
    else if (options.type == mailimap.StoreRemove) {
      paramString = '-X-GM-LABELS.SILENT';
    }
    paramString += ' (' + this._labelsToString(options.labels) + ')';
  }
  if (options.byuid) {
    this._socket.write(this._currentTag + ' uid store ' + this._indexSetToString(numbers) + ' ' + paramString + '\r\n', 'utf8');
  }
  else {
    this._socket.write(this._currentTag + ' store ' + this._indexSetToString(numbers) + ' ' + paramString + '\r\n', 'utf8');
  }
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      this._runCallback(null, items);
    }
    else {
      var error = new Error('store error');
      error.type = 'store_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._subscribe = function(folder, callback) {
  if (this._queueIfNeeded(this._subscribe, arguments)) {
    return;
  }
  
  if ((this._state == mailimap.CONNECTED) || (this._state == mailimap.DISCONNECTED)) {
    var error;
    if (this._state == mailimap.DISCONNECTED) {
      error = new Error('Not connected');
    }
    else {
      error = new Error('Not logged in');
    }
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' subscribe ' + this._toQuotedString(folder) + '\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      this._runCallback(null);
    }
    else {
      var error = new Error('subscribe error');
      error.type = 'subscribe_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._unsubscribe = function(folder, callback) {
  if (this._queueIfNeeded(this._unsubscribe, arguments)) {
    return;
  }
  
  if ((this._state == mailimap.CONNECTED) || (this._state == mailimap.DISCONNECTED)) {
    var error;
    if (this._state == mailimap.DISCONNECTED) {
      error = new Error('Not connected');
    }
    else {
      error = new Error('Not logged in');
    }
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' unsubcribe ' + this._toQuotedString(folder) + '\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      this._runCallback(null);
    }
    else {
      var error = new Error('unsubscribe error');
      error.type = 'subscribe_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._starttls = function(callback) {
  if (this._queueIfNeeded(this._starttls, arguments)) {
    return;
  }
  
  if (this._state != mailimap.CONNECTED) {
    var error;
    if (this._state == mailimap.DISCONNECTED) {
      error = new Error('Not connected');
    }
    else  {
      error = new Error('Already logged in');
    }
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' starttls\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      this._stopListening();
      this._switchConnectionToSSL()
    }
    else {
      var error = new Error('STARTTLS not available');
      error.type = 'starttls_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._switchConnectionToSSL = function() {
  var securePair = starttls.startTls(this._socket, function() {
    this._originalSocket = this._socket;
    this._socket = securePair.cleartext;
    this._runCallback(null);
  }.bind(this)); 
};

mailimap.IDLE_STATE_WAIT_NONE = -1;
mailimap.IDLE_STATE_WAIT_CONT_REQ = 0;
mailimap.IDLE_STATE_WAIT_RESPONSE = 1;
mailimap.IDLE_STATE_GOT_DATA = 2;
mailimap.IDLE_STATE_CANCELLED = 3;

mailimap.prototype._idle = function(callback) {
  if (this._queueIfNeeded(this._idle, arguments)) {
    return;
  }
  
  if (this._state != mailimap.SELECTED) {
    var error = new Error('Not logged in');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  if (this._idleState != mailimap.IDLE_STATE_WAIT_NONE) {
    var error = new Error('Already idling');
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  this._socket.write(this._currentTag + ' idle\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  this._idleState = mailimap.IDLE_STATE_WAIT_CONT_REQ;
  
  this._socket.on('data', function(data) {
    this._buffer = Buffer.concat([this._buffer, data]);
    if (this._idleState == mailimap.IDLE_STATE_WAIT_CONT_REQ) {
      console.log('try parse cont req');
      var r = etpan.responseParse(this._buffer, mailimap.PARSER_CONT_REQ);
      if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
        // Wait for more data.
      }
      else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
        //this._runCallback(null, items);
        if (r.hasIdleData) {
          this._socket.write('done\r\n', 'utf8');
          this._idleState = mailimap.IDLE_STATE_GOT_DATA;
        }
        else {
          this._idleState = mailimap.IDLE_STATE_WAIT_RESPONSE;
        }
      }
      else {
        var error = new Error('idle error');
        error.type = 'idle_error';
        this._runCallback(error);
      }
    }
    else if (this._idleState == mailimap.IDLE_STATE_WAIT_RESPONSE) {
      this._idleState = mailimap.IDLE_STATE_GOT_DATA;
      this._socket.write('done\r\n', 'utf8');
      
      var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
      if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
        // Wait for more data.
      }
      else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
        this._idleState = mailimap.IDLE_STATE_WAIT_NONE;
        this._runCallback(null);
      }
      else {
        var error = new Error('idle error');
        error.type = 'idle_error';
        this._runCallback(error);
      }
    }
    else if (this._idleState == mailimap.IDLE_STATE_GOT_DATA) {
      var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
      if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
        // Wait for more data.
      }
      else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
        this._idleState = mailimap.IDLE_STATE_WAIT_NONE;
        this._runCallback(null);
      }
      else {
        var error = new Error('idle error');
        error.type = 'idle_error';
        this._runCallback(error);
      }
    }
    else if (this._idleState == mailimap.IDLE_STATE_CANCELLED) {
      console.log('cancelled');
      var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
      if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
        // Wait for more data.
      }
      else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
        this._idleState = mailimap.IDLE_STATE_WAIT_NONE;
        this._runCallback(null);
      }
      else {
        var error = new Error('idle error');
        error.type = 'idle_error';
        this._runCallback(error);
      }
    }
  }.bind(this));
};

mailimap.prototype._idleDone = function() {
  if (this._idleState == mailimap.IDLE_STATE_WAIT_NONE) {
    // Was not idling
    return false;
  }
  
  if (this._idleState == mailimap.IDLE_STATE_WAIT_RESPONSE) {
    this._idleState = mailimap.IDLE_STATE_CANCELLED;
    this._socket.write('done\r\n', 'utf8');
  }
  
  return true;
};

mailimap.prototype._capabilitiesToString = function(capabilities) {
  var capString = '';
  capabilities.forEach(function(item, idx) {
    var capItemString = '';
    if (item == mailimap.CapabilityCondstore) {
      capItemString = 'CONDSTORE';
    }
    else if (item == mailimap.CapabilityQResync) {
      capItemString = 'QRESYNC';
    }
    if (capString.length == 0) {
      capString = capItemString;
    }
    else {
      capString += ' ' + capItemString;
    }
  });
  
  return capString;
};

mailimap.prototype._enable = function(capabilities, callback) {
  if (this._queueIfNeeded(this._enable, arguments)) {
    return;
  }
  
  if ((this._state == mailimap.CONNECTED) || (this._state == mailimap.DISCONNECTED)) {
    var error;
    if (this._state == mailimap.DISCONNECTED) {
      error = new Error('Not connected');
    }
    else {
      error = new Error('Not logged in');
    }
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  console.log(capabilities);
  console.log(this._currentTag + ' enable ' + this._capabilitiesToString(capabilities));
  this._currentTag ++;
  this._socket.write('02 enable ' + this._capabilitiesToString(capabilities) + '\r\n', 'utf8');
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    console.log(data.toString());
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      this._runCallback(null);
    }
    else {
      var error = new Error('enable error');
      error.type = 'enable_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._clientInfoToString = function(clientInfo) {
  var allKeys = Object.keys(clientInfo);
  if (allKeys.length == 0) {
    return 'NIL';
  }
  
  var result = '';
  allKeys.forEach(function(item, idx) {
    if (result.length == 0) {
      result = '(';
    }
    else {
      result += ' ';
    }
    
    result += this._toQuotedString(item) + ' ' + this._toQuotedString(clientInfo[item]);
  }.bind(this));
  if (result.length != 0) {
    result += ')';
  }
  
  return result;
};

mailimap.prototype._id = function(clientInfo, callback) {
  if (this._queueIfNeeded(this._id, arguments)) {
    return;
  }
  
  if ((this._state == mailimap.CONNECTED) || (this._state == mailimap.DISCONNECTED)) {
    var error;
    if (this._state == mailimap.DISCONNECTED) {
      error = new Error('Not connected');
    }
    else {
      error = new Error('Not logged in');
    }
    error.type = 'state_error';
    callback(error);
    return;
  }
  
  this._buffer = new Buffer(0);
  console.log(this._clientInfoToString(clientInfo));
  this._socket.write(this._currentTag + ' id ' + this._clientInfoToString(clientInfo) + '\r\n', 'utf8');
  this._currentTag ++;
  this._currentCallback = callback;
  
  this._socket.on('data', function(data) {
    console.log(data.toString());
    this._buffer = Buffer.concat([this._buffer, data]);
    var r = etpan.responseParse(this._buffer, mailimap.PARSER_ENABLE_RESPONSE);
    if (r.result == mailimap.MAILIMAP_ERROR_NEEDS_MORE_DATA) {
      // Wait for more data.
    }
    else if (r.result == mailimap.MAILIMAP_NO_ERROR) {
      var serverInfo = r.getIDResponseFromResponse();
      this._runCallback(null, serverInfo);
    }
    else {
      var error = new Error('id error');
      error.type = 'id_error';
      this._runCallback(error);
    }
  }.bind(this));
};

mailimap.prototype._toQuotedString = function(value) {
  value = value.replace(/\\/g, '\\\\').replace(/"/g, '\\"');
  return '"' + value + '"';
};

// Exports

module.exports = {
  mailimap: mailimap,
  imapset: imapset,
};

define('ConnectionClear', 0);
define('ConnectionSSL', 1);

define('CapabilityACL', 0);
define('CapabilityBinary', 1);
define('CapabilityCatenate', 2);
define('CapabilityChildren', 3);
define('CapabilityCompressDeflate', 4);
define('CapabilityCondstore', 5);
define('CapabilityEnable', 6);
define('CapabilityIdle', 7);
define('CapabilityId', 8);
define('CapabilityLiteralPlus', 9);
define('CapabilityMultiAppend', 10);
define('CapabilityNamespace', 11);
define('CapabilityQResync', 12);
define('CapabilityQuota', 13);
define('CapabilitySort', 14);
define('CapabilityStartTLS', 15);
define('CapabilityThreadOrderedSubject', 16);
define('CapabilityThreadReferences', 17);
define('CapabilityUIDPlus', 18);
define('CapabilityUnselect', 19);
define('CapabilityXList', 20);
define('CapabilityAuthAnonymous', 21);
define('CapabilityAuthCRAMMD5', 22);
define('CapabilityAuthDigestMD5', 23);
define('CapabilityAuthExternal', 24);
define('CapabilityAuthGSSAPI', 25);
define('CapabilityAuthKerberosV4', 26);
define('CapabilityAuthLogin', 27);
define('CapabilityAuthNTLM', 28);
define('CapabilityAuthOTP', 29);
define('CapabilityAuthPlain', 30);
define('CapabilityAuthSKey', 31);
define('CapabilityAuthSRP', 32);
define('CapabilityXOAuth2', 33);

define('FetchTypeAll', 0);
define('FetchTypeFull', 1);
define('FetchTypeFast', 2);
define('FetchTypeEnvelope', 3);
define('FetchTypeFlags', 4);
define('FetchTypeInternalDate', 5);
define('FetchTypeRFC822', 6);
define('FetchTypeRFC822Header', 7);
define('FetchTypeRFC822Size', 8);
define('FetchTypeRFC822Text', 9);
define('FetchTypeBody', 10);
define('FetchTypeBodyStructure', 11);
define('FetchTypeUID', 12);
define('FetchTypeBodySection', 13);
define('FetchTypeBodyPeekSection', 14);
define('FetchTypeModSeq', 15);
define('FetchTypeGmailThreadID', 16);
define('FetchTypeGmailMessageID', 17);
define('FetchTypeGmailLabels', 18);

define('MessageFlagNone', 0);
define('MessageFlagSeen', 1 << 0);
define('MessageFlagAnswered', 1 << 1);
define('MessageFlagFlagged', 1 << 2);
define('MessageFlagDeleted', 1 << 3);
define('MessageFlagDraft', 1 << 4);
define('MessageFlagMDNSent', 1 << 5);
define('MessageFlagForwarded', 1 << 6);
define('MessageFlagSubmitPending', 1 << 7);
define('MessageFlagSubmitted', 1 << 8);

define('StoreAdd', 1);
define('StoreRemove', -1);
define('StoreSet', 0);
