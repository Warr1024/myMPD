"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

function initPartitions() {
    document.getElementById('listPartitionsList').addEventListener('click', function(event) {
        event.stopPropagation();
        event.preventDefault();
        if (event.target.nodeName === 'A') {
            const action = event.target.getAttribute('data-action');
            const partition = decodeURI(event.target.parentNode.parentNode.getAttribute('data-partition'));
            if (action === 'delete') {
                deletePartition(partition);
            }
            else if (action === 'switch') {
                switchPartition(partition);
            }
        }
    }, false);
    
    document.getElementById('partitionOutputsList').addEventListener('click', function(event) {
        event.stopPropagation();
        event.preventDefault();
        if (event.target.nodeName === 'TD') {
            const outputName = decodeURI(event.target.parentNode.getAttribute('data-output'));
            moveOutput(outputName);
            uiElements.modalPartitionOutputs.hide();
        }
    }, false);

    document.getElementById('modalPartitions').addEventListener('shown.bs.modal', function () {
        showListPartitions();
        hideModalAlert();
        removeEnterPinFooter();
    });

    document.getElementById('modalPartitionOutputs').addEventListener('shown.bs.modal', function () {
        sendAPI("MYMPD_API_PLAYER_OUTPUT_LIST", {
            "partition": "default"
        }, parsePartitionOutputsList, true);
    });
}

function moveOutput(output) {
    sendAPI("MYMPD_API_PARTITION_OUTPUT_MOVE", {
        "name": output
    });
}

function parsePartitionOutputsList(obj) {
    const tbody = document.getElementById('partitionOutputsList');
    if (checkResult(obj, tbody, 1) === false) {
        return;
    }

    const outputs = document.getElementById('outputs').getElementsByTagName('button');
    const outputIds = [];
    for (let i = 0, j= outputs.length; i < j; i++) {
        outputIds.push(Number(outputs[i].getAttribute('data-output-id')));
    }

    let outputList = '';
    let nr = 0;
    for (let i = 0, j = obj.result.data.length; i < j; i++) {
        if (outputIds.includes(obj.result.data[i].id) === false) {
            outputList += '<tr data-output="' + encodeURI(obj.result.data[i].name) + '"><td>' +
                e(obj.result.data[i].name) + '</td></tr>';
            nr++;
        }
    }
    if (nr === 0) {
        outputList = '<tr class="not-clickable"><td><span class="mi">info</span>&nbsp;&nbsp;' + t('Empty list') + '</td></tr>';
    }
    tbody.innerHTML = outputList;
}

//eslint-disable-next-line no-unused-vars
function savePartition() {
    let formOK = true;
    
    const nameEl = document.getElementById('inputPartitionName');
    if (!validatePlnameEl(nameEl)) {
        formOK = false;
    }
    
    if (formOK === true) {
        sendAPI("MYMPD_API_PARTITION_NEW", {
            "name": nameEl.value
            }, savePartitionCheckError, true);
    }
}

function savePartitionCheckError(obj) {
    removeEnterPinFooter();
    if (obj.error) {
        showModalAlert(obj);
    }
    else {
        hideModalAlert();
        showListPartitions();
    }
}

//eslint-disable-next-line no-unused-vars
function showNewPartition() {
    document.getElementById('listPartitions').classList.remove('active');
    document.getElementById('newPartition').classList.add('active');
    document.getElementById('listPartitionsFooter').classList.add('hide');
    document.getElementById('newPartitionFooter').classList.remove('hide');
    
    removeIsInvalid(document.getElementById('modalPartitions'));
    const nameEl = document.getElementById('inputPartitionName');
    nameEl.value = '';
    nameEl.focus();
}

function showListPartitions() {
    document.getElementById('listPartitions').classList.add('active');
    document.getElementById('newPartition').classList.remove('active');
    document.getElementById('listPartitionsFooter').classList.remove('hide');
    document.getElementById('newPartitionFooter').classList.add('hide');
    sendAPI("MYMPD_API_PARTITION_LIST", {}, parsePartitionList, true);
}

function deletePartition(partition) {
    sendAPI("MYMPD_API_PARTITION_RM", {
        "name": partition
    }, savePartitionCheckError, true);
}

function switchPartition(partition) {
    sendAPI("MYMPD_API_PARTITION_SWITCH", {
        "name": partition
    }, function(obj) {
        savePartitionCheckError(obj);
        sendAPI("MYMPD_API_PLAYER_STATE", {}, parseState);
    }, true);
}

function parsePartitionList(obj) {
    const tbody = document.getElementById('listPartitionsList');
    if (checkResult(obj, tbody, 3) === false) {
        return;
    }

    let partitionList = '';
    for (let i = 0, j = obj.result.data.length; i < j; i++) {
        partitionList += '<tr data-partition="' + encodeURI(obj.result.data[i].name) + '"><td class="' +
            (obj.result.data[i].name === settings.partition ? 'font-weight-bold' : '') +
            '">' + e(obj.result.data[i].name) + 
            (obj.result.data[i].name === settings.partition ? '&nbsp;(' + t('current') + ')' : '') +
            '</td>' +
            '<td data-col="Action">' +
            (obj.result.data[i].name === 'default' || obj.result.data[i].name === settings.partition  ? '' : 
                '<a href="#" title="' + t('Delete') + '" data-action="delete" class="mi color-darkgrey">delete</a>') +
            (obj.result.data[i].name !== settings.partition ? '<a href="#" title="' + t('Switch to') + '" data-action="switch" class="mi color-darkgrey">check_circle</a>' : '') +
            '</td></tr>';
    }
    tbody.innerHTML = partitionList;
}
