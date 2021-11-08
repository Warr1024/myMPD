"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

function initBrowse() {
    document.getElementById('BrowseDatabaseListList').addEventListener('click', function(event) {
        if (event.target.classList.contains('row')) {
            return;
        }
        if (app.current.tag === 'Album') {
            if (event.target.classList.contains('card-body')) {
                appGoto('Browse', 'Database', 'Detail', 0, undefined, 'Album', 'AlbumArtist', 
                    getData(event.target.parentNode, 'data-album'),
                    getData(event.target.parentNode, 'data-albumartist')
                );
            }
            else if (event.target.classList.contains('card-footer')){
                popoverMenuAlbumCards(event);
            }
        }
        else {
            app.current.search = '';
            document.getElementById('searchDatabaseStr').value = '';
            appGoto(app.current.card, app.current.card, undefined, 0, undefined, 'Album', 'AlbumArtist', 'Album', 
                '((' + app.current.tag + ' == \'' + escapeMPD(getData(event.target.parentNode, 'data-tag')) + '\'))');
        }
    }, false);
    
    document.getElementById('BrowseDatabaseListList').addEventListener('contextmenu', function(event) {
        if (event.target.classList.contains('row') || event.target.parentNode.classList.contains('not-clickable')) {
            return;
        }
        if (app.current.tag === 'Album') {
            popoverMenuAlbumCards(event);
        }
    }, false);
    
    document.getElementById('BrowseDatabaseListList').addEventListener('long-press', function(event) {
        if (event.target.classList.contains('row')) {
            return;
        }
        if (app.current.tag === 'Album') {
            popoverMenuAlbumCards(event);
        }
    }, false);
   
    document.getElementById('BrowseDatabaseDetailList').addEventListener('click', function(event) {
        if (event.target.parentNode.parentNode.nodeName === 'TFOOT') {
            return;
        }
        if (event.target.nodeName === 'TD') {
            clickSong(getData(event.target.parentNode, 'data-uri'), getData(event.target.parentNode, 'data-name'));
        }
        else if (event.target.nodeName === 'A') {
            showPopover(event);
        }
    }, false);

    document.getElementById('searchDatabaseTags').addEventListener('click', function(event) {
        if (event.target.nodeName === 'BUTTON') {
            app.current.filter = getData(event.target, 'data-tag');
            searchAlbumgrid(document.getElementById('searchDatabaseStr').value);
        }
    }, false);
    
    document.getElementById('databaseSortDesc').addEventListener('click', function(event) {
        toggleBtnChk(this);
        event.stopPropagation();
        event.preventDefault();
        if (app.current.sort.charAt(0) === '-') {
            app.current.sort = app.current.sort.substr(1);
        }
        else {
            app.current.sort = '-' + app.current.sort;
        }
        appGoto(app.current.card, app.current.tab, app.current.view, 0, app.current.limit, app.current.filter, app.current.sort, app.current.tag, app.current.search);
    }, false);

    document.getElementById('databaseSortTags').addEventListener('click', function(event) {
        if (event.target.nodeName === 'BUTTON') {
            event.preventDefault();
            event.stopPropagation();
            app.current.sort = getData(event.target, 'data-tag');
            appGoto(app.current.card, app.current.tab, app.current.view, 0, app.current.limit, app.current.filter, app.current.sort, app.current.tag, app.current.search);
        }
    }, false);

    document.getElementById('BrowseDatabaseByTagDropdown').addEventListener('click', function(event) {
        navBrowseHandler(event);
    }, false);
    
    document.getElementById('BrowseNavPlaylistsDropdown').addEventListener('click', function(event) {
        navBrowseHandler(event);
    }, false);
    
    document.getElementById('BrowseNavFilesystemDropdown').addEventListener('click', function(event) {
        navBrowseHandler(event);
    }, false);

    document.getElementById('dropdownSortPlaylistTags').addEventListener('click', function(event) {
        if (event.target.nodeName === 'BUTTON') {
            event.preventDefault();
            playlistSort(getData(event.target, 'data-tag'));
        }
    }, false);

    document.getElementById('searchFilesystemStr').addEventListener('keyup', function(event) {
        if (event.key === 'Escape') {
            this.blur();
        }
        else {
            appGoto(app.current.card, app.current.tab, app.current.view, 
                0, app.current.limit, (this.value !== '' ? this.value : '-'), app.current.sort, '-', app.current.search);
        }
    }, false);
    
    document.getElementById('searchDatabaseStr').addEventListener('keyup', function(event) {
        if (event.key === 'Escape') {
            this.blur();
        }
        else if (event.key === 'Enter' && app.current.tag === 'Album') {
            if (this.value !== '') {
                const op = getSelectValueId('searchDatabaseMatch');
                const crumbEl = document.getElementById('searchDatabaseCrumb');
                crumbEl.appendChild(createSearchCrumb(app.current.filter, op, this.value));
                elShow(crumbEl);
                this.value = '';
            }
            else {
                searchAlbumgrid(this.value);
            }
        }
        else if (app.current.tag === 'Album') {
            searchAlbumgrid(this.value);
        }
        else {
            appGoto(app.current.card, app.current.tab, app.current.view, 
                0, app.current.limit, app.current.filter, app.current.sort, app.current.tag, this.value);
        }
    }, false);

    document.getElementById('searchDatabaseMatch').addEventListener('change', function() {
        searchAlbumgrid(document.getElementById('searchDatabaseStr').value);
    });

    document.getElementById('searchDatabaseCrumb').addEventListener('click', function(event) {
        if (event.target.nodeName === 'SPAN') {
            //remove search expression
            event.preventDefault();
            event.stopPropagation();
            event.target.parentNode.remove();
            searchAlbumgrid('');
        }
        else if (event.target.nodeName === 'BUTTON') {
            //edit search expression
            event.preventDefault();
            event.stopPropagation();
            selectTag('searchDatabaseTags', 'searchDatabaseTagsDesc', getData(event.target,'data-filter-tag'));
            document.getElementById('searchDatabaseStr').value = unescapeMPD(getData(event.target, 'data-filter-value'));
            document.getElementById('searchDatabaseMatch').value = getData(event.target, 'data-filter-op');
            event.target.remove();
            app.current.filter = getData(event.target,'data-filter-tag');
            searchAlbumgrid(document.getElementById('searchDatabaseStr').value);
            if (document.getElementById('searchDatabaseCrumb').childElementCount === 0) {
                elHideId('searchDatabaseCrumb');
            }
        }
    }, false);

    document.getElementById('BrowseFilesystemList').addEventListener('click', function(event) {
        let target;
        switch(event.target.nodeName) {
             case 'TD': target = event.target.parentNode; break;
             case 'DIV': target = event.target.parentNode; break;
             case 'SPAN':
             case 'SMALL': target = event.target.parentNode.parentNode.parentNode; break;
             default: target = event.target;
        }
        if (target.nodeName === 'TR') {
            const uri = getData(target, 'data-uri');
            const dataType = getData(target, 'data-type');
            switch(dataType) {
                case 'parentDir': {
                    const offset = browseFilesystemHistory[uri] !== undefined ? browseFilesystemHistory[uri].offset : 0;
                    const scrollPos = browseFilesystemHistory[uri] !== undefined ? browseFilesystemHistory[uri].scrollPos : 0;
                    app.current.filter = '-';
                    appGoto('Browse', 'Filesystem', undefined, offset, app.current.limit, app.current.filter, app.current.sort, '-', uri, scrollPos);
                    break;
                }
                case 'dir':
                    clickFolder(uri);
                    break;
                case 'song':
                    clickSong(uri);
                    break;
                case 'plist':
                    clickPlaylist(uri);
                    break;
            }
        }
        else if (target.nodeName === 'A') {
            showPopover(event);
        }
    }, false);

    document.getElementById('BrowseBreadcrumb').addEventListener('click', function(event) {
        if (event.target.nodeName === 'A') {
            event.preventDefault();
            const uri = getData(event.target, 'data-uri');
            const offset = browseFilesystemHistory[uri] !== undefined ? browseFilesystemHistory[uri].offset : 0;
            const scrollPos = browseFilesystemHistory[uri] !== undefined ? browseFilesystemHistory[uri].scrollPos : 0;
            appGoto('Browse', 'Filesystem', undefined, offset, app.current.limit, app.current.filter, app.current.sort, '-', uri, scrollPos);
        }
    }, false);
}

function navBrowseHandler(event) {
    if (event.target.nodeName === 'BUTTON') {
        const tag = getData(event.target, 'data-tag');
        if (tag === 'Playlists' || tag === 'Filesystem') {
            appGoto('Browse', tag, undefined);
            return;
        }
        
        if (app.current.card === 'Browse' && app.current.tab !== 'Database') {
            appGoto('Browse', 'Database', app.cards.Browse.tabs.Database.active);
            return;
        }
        if (tag !== 'Album') {
            app.current.filter = tag;
            app.current.sort = tag;
        }
        app.current.search = '';
        document.getElementById('searchDatabaseMatch').value = 'contains';
        appGoto(app.current.card, app.current.tab, app.current.view, 
            0, app.current.limit, app.current.filter, app.current.sort, tag, app.current.search);
    }
}

function popoverMenuAlbumCards(event) {
    if (event.target.classList.contains('row')) {
        return;
    }
    showPopover(event);
    const selCards = document.getElementById('BrowseDatabaseListList').getElementsByClassName('selected');
    for (let i = 0, j = selCards.length; i < j; i++) {
        selCards[i].classList.remove('selected');
    }
    event.target.parentNode.classList.add('selected');
    event.preventDefault();
    event.stopPropagation();
}

function gotoBrowse(event) {
    if (features.featAdvsearch === false) {
        return;
    }
    const x = event.target;
    let tag = getData(x, 'data-tag');
    let name = getData(x, 'data-name');
    if (tag === null) {
        tag = getData(x.parentNode, 'data-tag');
        name = getData(x.parentNode, 'data-name');
    }
    if (tag !== '' && name !== '' && name !== '-' && settings.tagListBrowse.includes(tag)) {
        if (tag === 'Album') {
            let artist = getData(x, 'data-albumartist');
            if (artist === null) {
                artist = getData(x.parentNode, 'data-albumartist');
            }
            if (artist !== null) {
                //Show album details
                appGoto('Browse', 'Database', 'Detail', 0, undefined, tag, tagAlbumArtist, name, artist);
            }
            else {
                //show filtered album list
                gotoAlbumList(tag, name);
            }
        }
        else {
            //show filtered album list
            gotoAlbumList(tag, name);
        }
    }
}

//eslint-disable-next-line no-unused-vars
function gotoAlbum(artist, album) {
    appGoto('Browse', 'Database', 'Detail', 0, undefined, 'Album', tagAlbumArtist, album, artist);
}

//eslint-disable-next-line no-unused-vars
function gotoAlbumList(tag, value) {
    document.getElementById('searchDatabaseStr').value = '';
    let expression = '(';
    for (let i = 0, j = value.length; i < j; i++) {
        if (i > 0) {
            expression += ' AND '
        }
        expression += '(' + tag + ' == \'' + escapeMPD(value[i]) + '\')';
    }
    expression += ')';
    appGoto('Browse', 'Database', 'List', 0, undefined, tag, tagAlbumArtist, 'Album', expression);
}

//eslint-disable-next-line no-unused-vars
function gotoFilesystem(uri) {
    document.getElementById('searchFilesystemStr').value = '';
    appGoto('Browse', 'Filesystem', undefined, 0, undefined, '-','-','-', uri);
}

function parseFilesystem(obj) {
    //show images in folder
    const imageList = document.getElementById('BrowseFilesystemImages');
    elClear(imageList);

    const table = document.getElementById('BrowseFilesystemList');
    const tfoot = table.getElementsByTagName('tfoot')[0];
    elClear(tfoot);

    if (checkResult(obj, 'BrowseFilesystem', null) === false) {
        elHide(imageList);
        return;
    }

    if ((obj.result.images.length === 0 && obj.result.bookletPath === '')) {
        elHide(imageList);
    }
    else {
        elShow(imageList);
    }
    if (obj.result.bookletPath !== '') {
        const img = document.createElement('div');
        img.style.backgroundImage = 'url("' + subdir + '/assets/coverimage-booklet.svg")';
        img.classList.add('booklet');
        setData(img, 'data-href', subdir + '/browse/music/' + myEncodeURI(obj.result.bookletPath));
        img.title = tn('Booklet');
        imageList.appendChild(img);
    }
    for (let i = 0, j = obj.result.images.length; i < j; i++) {
        const img = document.createElement('div');
        img.style.backgroundImage = 'url("' + subdir + '/browse/music/' + myEncodeURI(obj.result.images[i]) + '"),url("assets/coverimage-loading.svg")';
        imageList.appendChild(img);
    }

    const rowTitleSong = webuiSettingsDefault.clickSong.validValues[settings.webuiSettings.clickSong];
    const rowTitleFolder = webuiSettingsDefault.clickFolder.validValues[settings.webuiSettings.clickFolder];
    const rowTitlePlaylist = webuiSettingsDefault.clickPlaylist.validValues[settings.webuiSettings.clickPlaylist];
    
    updateTable(obj, 'BrowseFilesystem', function(row, data) {
        setData(row, 'data-type', data.Type);
        setData(row, 'data-uri', data.uri);
        //set Title to name if not defined - for folders and playlists
        if (data.Title === undefined) {
            data.Title = data.name;
        }
        setData(row, 'data-name', data.Title);
        row.setAttribute('title', tn(data.Type === 'song' ? rowTitleSong : 
                data.Type === 'dir' ? rowTitleFolder : rowTitlePlaylist));
    });
    scrollToPosY(app.current.scrollPos);

    const colspan = settings.colsBrowseFilesystem.length + 1;
    tfoot.appendChild(
        elCreateNode('tr', {},
            elCreateText('td', {"colspan": colspan}, tn('Num entries', obj.result.totalEntities))
        )
    );
}

function parseDatabase(obj) {
    const cardContainer = document.getElementById('BrowseDatabaseListList');
    setScrollViewHeight(cardContainer);
    const cols = cardContainer.getElementsByClassName('col');
    document.getElementById('BrowseDatabaseListList').classList.remove('opacity05');

    if (obj.error !== undefined) {
        elClear(cardContainer);
        cardContainer.appendChild(
            elCreateText('div', {"class": ["col", "not-clickable", "alert", "alert-danger"]}, tn(obj.error.message, obj.error.data))
        );
        setPagination(0, 0);
        return;
    }

    const nrItems = obj.result.returnedEntities;
    if (nrItems === 0) {
        elClear(cardContainer);
        cardContainer.appendChild(
            elCreateEmpty('div', {"class": ["col", "not-clickable", "alert", "alert-secondary"]}, tn('Empty list'))
        );
        setPagination(0, 0);
        return;
    }

    if (cardContainer.getElementsByClassName('not-clickable').length > 0) {
        elClear(cardContainer);
    }
    for (let i = 0; i < nrItems; i++) {
        if (obj.result.data[i].AlbumArtist === '') {
            obj.result.data[i].AlbumArtist = tn('Unknown artist');
        }
        if (obj.result.data[i].Album === '') {
            obj.result.data[i].Album = tn('Unknown album');
        }
        const id = obj.result.tag === 'Album' ? genId('database' + obj.result.data[i].Album + obj.result.data[i].AlbumArtist)
                                              : genId('database' + obj.result.data[i].value);

        if (cols[i] !== undefined && cols[i].firstChild.firstChild.getAttribute('id') === id) {
            continue;
        }

        let picture = '';
        const col = elCreateEmpty('div', {"class": ["col", "px-0", "flex-grow-0"]});
        const card = elCreateEmpty('div', {"class": ["card", "card-grid", "clickable"], "tabindex": 0});
        if (obj.result.tag === 'Album') {
            picture = subdir + '/albumart/' + obj.result.data[i].FirstSongUri;
        
            const cardBody = elCreateEmpty('div', {"class": ["card-body", "album-cover-loading", "album-cover-grid", "d-flex"], "id": id});
            const cardFooter = elCreateNode('div', {"class": ["card-footer", "card-footer-grid", "p-2"], 
                "title": obj.result.data[i][tagAlbumArtist] + ': ' + obj.result.data[i].Album}, 
                printValue('Album', obj.result.data[i].Album));
            cardFooter.appendChild(elCreateEmpty('br', {}));
            cardFooter.appendChild(elCreateNode('small', {}, printValue(tagAlbumArtist, obj.result.data[i][tagAlbumArtist])));
            card.appendChild(cardBody);
            card.appendChild(cardFooter);
            setData(card, 'data-picture', picture);
            setData(card, 'data-uri', obj.result.data[i].FirstSongUri.replace(/\/[^/]+$/, ''));
            setData(card, 'data-type', 'album');
            setData(card, 'data-name', obj.result.data[i].Album);
            setData(card, 'data-album', obj.result.data[i].Album);
            setData(card, 'data-albumartist', obj.result.data[i].AlbumArtist);
            addPlayButton(cardBody);
        }
        else {
            picture = subdir + '/tagart/' + obj.result.tag + '/' + obj.result.data[i].value;

            if (obj.result.pics === true) {
                const cardBody = elCreateEmpty('div', {"class": ["card-body", "album-cover-loading", "album-cover-grid", "d-flex"], "id": id});
                card.appendChild(cardBody);
            }
            
            const cardFooter = elCreateText('div', {"class": ["card-footer", "card-footer-grid", "p-2"], 
                "title": obj.result.data[i].value}, obj.result.data[i].value);
            card.appendChild(cardFooter);
            setData(card, 'data-picture', picture);
            setData(card, 'data-tag', obj.result.data[i].value);
        }
        col.appendChild(card);
        i < cols.length ? cols[i].replaceWith(col) : cardContainer.append(col);

        if (hasIO === true) {
            const options = {
                root: null,
                rootMargin: '0px',
            };
            const observer = new IntersectionObserver(setGridImage, options);
            observer.observe(col);
        }
        else {
            col.firstChild.firstChild.style.backgroundImage = myEncodeURI(picture);
        }
    }
    for (let i = cols.length - 1; i >= nrItems; i--) {
        cols[i].remove();
    }
    
    setPagination(obj.result.totalEntities, obj.result.returnedEntities);    
}

function setGridImage(changes, observer) {
    changes.forEach(change => {
        if (change.intersectionRatio > 0) {
            observer.unobserve(change.target);
            const uri = getData(change.target.firstChild, 'data-picture');
            const body = change.target.firstChild.getElementsByClassName('card-body')[0];
            if (body) {
                body.style.backgroundImage = 'url("' + myEncodeURI(uri) + '"), url("' + subdir + '/assets/coverimage-loading.svg")';
            }
        }
    });
}

function addPlayButton(parentEl) {
    const div = document.createElement('div');
    div.classList.add('align-self-end', 'album-grid-mouseover', 'mi', 'rounded-circle', 'clickable');
    div.textContent = 'play_arrow';
    div.title = tn(webuiSettingsDefault.clickAlbumPlay.validValues[settings.webuiSettings.clickAlbumPlay]);
    parentEl.appendChild(div);
    div.addEventListener('click', function(event) {
        event.preventDefault();
        event.stopPropagation();
        clickAlbumPlay(getData(event.target.parentNode.parentNode, 'data-albumartist'), getData(event.target.parentNode.parentNode, 'data-album'));
    }, false);
}

function parseAlbumDetails(obj) {
    const table = document.getElementById('BrowseDatabaseDetailList');
    const tfoot = table.getElementsByTagName('tfoot')[0];
    const colspan = settings.colsBrowseDatabaseDetail.length;
    const infoEl = document.getElementById('viewDetailDatabaseInfo');

    if (checkResult(obj, null, 3) === false) {
        elClear(infoEl);
        elClear(tfoot);
        return;
    }

    const coverEl = document.getElementById('viewDetailDatabaseCover');
    coverEl.style.backgroundImage = 'url("' + subdir + '/albumart/' + myEncodeURI(obj.result.data[0].uri) + '"), url("' + subdir + '/assets/coverimage-loading.svg")';
    setData(coverEl, 'data-images', obj.result.images);
    setData(coverEl, 'data-uri', obj.result.data[0].uri);
    
    elClear(infoEl);
    infoEl.appendChild(elCreateText('h1', {}, obj.result.Album));
    infoEl.appendChild(elCreateText('small', {}, tn('AlbumArtist')));
    const p = elCreateEmpty('p', {}, '');
    
    if (settings.tagListBrowse.includes(tagAlbumArtist)) {
        for (const artist of obj.result.AlbumArtist) {
            const artistLink = elCreateText('a', {"href": "#"}, artist);
            setData(artistLink, 'data-tag', tagAlbumArtist);
            setData(artistLink, 'data-name', artist);
            artistLink.addEventListener('click', function(event) {
                event.preventDefault();
                gotoBrowse(event);
            }, false);
            p.appendChild(artistLink);
            p.appendChild(elCreateEmpty('br', {}));
        }
    }
    else {
        p.textContent.appendChild(printValue('AlbumArtist', obj.result.AlbumArtist));
    }
    infoEl.appendChild(p);
    if (obj.result.bookletPath !== '' && features.featLibrary === true) {
        const booklet = elCreateEmpty('p', {});
        booklet.appendChild(elCreateText('span', {"class": ["mi", "me-2"]}, 'description'));
        booklet.appendChild(elCreateText('a', {"target": "_blank", "href": subdir + '/browse/music/' + 
            myEncodeURI(obj.result.bookletPath)}, tn('Download booklet')));
        infoEl.appendChild(booklet);
    }
    
    const rowTitle = tn(webuiSettingsDefault.clickSong.validValues[settings.webuiSettings.clickSong]);
    updateTable(obj, 'BrowseDatabaseDetail', function(row, data) {
        setData(row, 'data-type', 'song');
        setData(row, 'data-name', data.Title);
        setData(row, 'data-uri', data.uri);
        row.setAttribute('title', rowTitle);
    });

    const tr = elCreateEmpty('tr', {});
    const td = elCreateEmpty('td', {"colspan": colspan + 1});
    const small = elCreateEmpty('small', {});
    small.appendChild(elCreateText('span', {}, tn('Num songs', obj.result.totalEntities)));
    small.appendChild(elCreateText('span', {}, ' – '));
    small.appendChild(elCreateText('span', {}, beautifyDuration(obj.result.totalTime)));
    td.appendChild(small);
    tr.appendChild(td);
    elClear(tfoot);
    tfoot.appendChild(tr);
}

//eslint-disable-next-line no-unused-vars
function backToAlbumGrid() {
    appGoto('Browse', 'Database', 'List');
}  

//eslint-disable-next-line no-unused-vars
function addAlbum(action) {
    const album = app.current.tag;
    const albumArtist = app.current.search;
    _addAlbum(action, albumArtist, album);
}

//eslint-disable-next-line no-unused-vars
function appendQueueAlbum(albumArtist, album) {
    _addAlbum('appendQueue', albumArtist, album, undefined);
}

//eslint-disable-next-line no-unused-vars
function replaceQueueAlbum(albumArtist, album) {
    _addAlbum('replaceQueue', albumArtist, album, undefined);
}

//eslint-disable-next-line no-unused-vars
function insertQueueAlbum(albumArtist, album) {
    _addAlbum('insertQueue', albumArtist, album, undefined);
}

//eslint-disable-next-line no-unused-vars
function playQueueAlbum(albumArtist, album) {
    _addAlbum('replaceQueue', albumArtist, album, undefined);
}

function _addAlbum(action, albumArtist, album, disc) {
    let expression = '((Album == \'' + escapeMPD(album) + '\')';
    for (const artist of albumArtist) {
        expression += ' AND (' + tagAlbumArtist + ' == \'' + escapeMPD(artist) + '\')';
    }
    if (disc !== undefined) {
        expression += ' AND (Disc == \'' + escapeMPD(disc) + '\')';
    }
    expression += ')';

    switch(action) {
        case 'appendQueue':
            appendQueue('search', expression);
            break;
        case 'replaceQueue':
            replaceQueue('search', expression);
            break;
        case 'insertQueue':
            insertQueue('search', expression, 0, 1, false);
            break;
        case 'playQueue':
            insertQueue('search', expression, 0, 1, true);
            break;
        case 'addPlaylist':
            showAddToPlaylist('ALBUM', expression);
            break;
    }
}

function searchAlbumgrid(x) {
    const expression = createSearchExpression(document.getElementById('searchDatabaseCrumb'), app.current.filter, getSelectValueId('searchDatabaseMatch'), x);
    appGoto(app.current.card, app.current.tab, app.current.view, 
        0, app.current.limit, app.current.filter, app.current.sort, app.current.tag, expression, 0);
}
