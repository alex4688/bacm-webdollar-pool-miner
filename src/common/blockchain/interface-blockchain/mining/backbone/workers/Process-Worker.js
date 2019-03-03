var exec = require('child_process').exec;
var fs = require('fs');
var NodeIPC = require('node-ipc').IPC;

import consts from "consts/const_global"
const uuid = require('uuid');
import  Utils from "common/utils/helpers/Utils"

import AdvancedEmitter from "common/utils/Advanced-Emitter";
import Blockchain from "main-blockchain/Blockchain"

import Log from 'common/utils/logging/Log';

class ProcessWorker{

    constructor(id, noncesWorkBatch, allowSendBeforeReadPreviously=true){


        this.id = id||0;
        this.noncesWorkBatch = noncesWorkBatch;
        this.allowSendBeforeReadPreviously = allowSendBeforeReadPreviously;

        this.suffix = this.id;

        this._child = undefined;

        this.emitter = new AdvancedEmitter(100);

        this._is_batching = false;
        this._path = '';

        this._start = 0;
        this._end = -1;
        this._data = undefined;
        this._connected = false;

        this._lastData = undefined;
        this._nextData = undefined;

        this.socketId = 'argon2-miner';
        this.ipc = new NodeIPC;
    }

    _getProcessParams(){
        return this._path;
    }

    restartWorker(){
        this._is_batching = false;
    }

    async start(path, filename) {
        if (path !== undefined)
            this._path = path + filename;


        this._child = exec(this._getProcessParams(), async (e, stdout, stderr) => {
            if (e) {
                console.error("Process Raised an error", e);
            }
        });

        if (this._child.exitCode !== null)
            return false;

        this._child.stdout.on('data', (data) => {
            console.log(data);
        });

        this._child.stderr.on('data', (data) => {
            console.log(data);
        });

        this._child.on('close', () => {
            console.log('done');
            console.info();
        });

        await Blockchain.blockchain.sleep(2000);

        this.ipc.config.socketRoot = '/tmp/';
        this.ipc.config.appspace = '';
        this.ipc.config.retry = 1500;
        this.ipc.config.rawBuffer = true;
        this.ipc.config.silent = true;

        this.ipc.connectToNet(this.socketId, 'localhost', 3456);
        var self = this;

        this.ipc.of[this.socketId].on(
            'connect',
            function() {
                console.log("Connected!!");
                self.connected = true;
            }
        );

        this.ipc.of[this.socketId].on(
            'disconnect',
            function() {
                console.log("Disconnected!!");
                self.connected = false;
            }
        );

        this.ipc.of[this.socketId].on('data',
            function(data) {
                self._validateWork(data);
            }
        );

        this._prevHash = '';
        await Blockchain.blockchain.sleep(1000);
        return true;
    }

    set connected(value) {
        this._connected = value;
    }

    get connected() {
        return this._connected;
    }

    kill(param){

        clearTimeout(this._timeoutValidation);
        this._child.kill(param);

    }

    async send(length, block, difficulty, start, end) {

        try {
            let buffer_size = 8 + length + 32 + 4;
            var msg = Buffer.alloc(4 + buffer_size);

            let offset = 0;
            msg.writeUInt32LE(buffer_size, offset); offset += 4;
            msg.writeUInt32LE(start, offset); offset += 4;
            msg.writeUInt32LE(length, offset); offset += 4;
            block.copy(msg, offset, 0, length); offset += length;
            difficulty.copy(msg, offset, 0, 32); offset += 32;
            msg.writeUInt32LE(end, offset);

            this._timeStart = new Date().getTime();
            this._count = end - start;

            this._start = start;
            this._end = end;

            this._data = msg;

            if (this._lastData === undefined ){
                this._lastData = msg;
                this.ipc.of[this.socketId].emit(msg);
            }
            else
                this._nextData = msg;
        }
        catch (exception) {
            console.log("Error sending data to be mined", exception);
            console.log(length, block, difficulty, start,end);
        }
    }

    async _sendKill(){

        try {
            let buffer_size = 8;
            var msg = Buffer.alloc(4 + buffer_size);

            let offset = 0;
            msg.writeUInt32LE(buffer_size, offset); offset += 4;
            msg.writeUInt32LE(0, offset); offset += 4;
            msg.writeUInt32LE(0, offset); offset += 4;
            this.ipc.of[this.socketId].emit(msg);
        } catch (exception){
            console.error("Error sending the data to miner", exception);
        }
    }


    /*async _writeWork(data){

        try {

            await fs.writeFileSync( this._outputFilename + this.suffix, data, "binary");
        } catch (exception){
            console.error("Error sending the data to GPU", exception);
            this._sendDataTimeout = setTimeout( this._writeWork.bind(this,data), 10 );
        }

    }*/



    /*async _deleteFile(prefix = ''){

        if (false === await fs.existsSync(this._outputFilename + this.suffix + prefix ))
            return;

        try {
            await fs.unlinkSync(this._outputFilename + this.suffix + prefix );
        } catch (exception){
        }

    }*/

    // async _writeWork(){
    //
    //     if (this._data !== undefined)
    //         try {
    //
    //             await this._deleteFile();
    //
    //             await fs.writeFileSync( this._outputFilename + this.suffix, this._data, "binary");
    //             this._data = undefined;
    //
    //         } catch (exception){
    //             console.error("Error sending the data to GPU", exception);
    //         }
    //     this._sendDataTimeout = setTimeout( this._writeWork.bind(this), 10 );
    //
    // }


    async _validateWork(data){

        try {
            data = JSON.parse(data.toString());

            let hash;
            if (data.bestHash !== undefined) hash = data.bestHash;
            else hash = data.hash;

            let nonce;
            if (data.bestNonce !== undefined) nonce = data.bestNonce;
            else nonce = data.nonce;

            if (hash !== this._prevHash) {

                console.info("Hashrate: " + data.h + " h/s");

                if ( data.type === "b" || data.type === "s")
                    data.h = this._count;

                this._emit("message", data);

                this._prevHash = hash;

                if (this._nextData !== undefined) {
                    this.ipc.of[this.socketId].emit(this._nextData);
                    this._nextData = undefined;
                }
                else
                    this._lastData = undefined;
            }
        } catch (exception){
            console.log("Error on validateWork: ", exception);

        }
    }


    _emit(a,b){
        return this.emitter.emit(a,b)
    }

    on(a,b){
        return this.emitter.on(a,b);
    }

    once(a,b){
        return this.emitter.once(a,b);
    }

    /*get connected(){
        return true;
    }*/

}

export default ProcessWorker