/*

A JavaScript Blackjack game created June 2013 by Chris Clower 
(clowerweb.com). Deck class loosely based on a tutorial at:
http://www.codecademy.com/courses/blackjack-part-1

All graphics and code were designed/written by me except for the
chip box on the table, which was taken from the image at:
http://www.marketwallpapers.com/wallpapers/9/wallpaper-52946.jpg

Uses Twitter Bootstrap and jQuery, which also were not created by
me :)

Fonts used:
* "Blackjack" logo: Exmouth
* Symbol/floral graphics: Dingleberries
* All other fonts: Adobe Garamond Pro

All graphics designed in Adobe Fireworks CS6

You are free to use or modify this code for any purpose, but I ask
that you leave this comment intact. Please understand that this is
still very much a work in progress, and is not feature complete nor
without bugs.

I will also try to comment the code better for future updates :D

*/

/*global $, confirm, Game, Player, renderCard, Card, setActions, 
resetBoard, showBoard, showAlert, getWinner, jQuery, wager */

(function () {
	var autoplay = true, inmotion = false;
/*****************************************************************/
/*************************** Globals *****************************/
/*****************************************************************/
    let player_pos = [{x:750, y:235},
		{x:50 , y:120}, {x:50 , y:260}, {x:50, y:400}, {x:50, y:560}, {x:50, y:700},
		{x:225, y:800}, {x:425, y:800}, {x:625, y:800}, {x:825, y:800},{x:1025, y:800}, {x:1225, y:800},
		{x:1600, y:700}, {x:1600, y:560}, {x:1600, y:400}, {x:1600, y:260}, {x:1600, y:120}
    ];

    let handdata = {"players":[{"name":"Dealer","cards":"K5K","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 0","cards":"93T","wager":"9","bank":"92","handResult":"1"},
			{"name":"Team 1","cards":"A9","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 2","cards":"J7","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 3","cards":"KQ","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 4","cards":"75","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 5","cards":"55","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 6","cards":"T7","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 7","cards":"Q3","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 8","cards":"8A","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 9","cards":"24","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 10","cards":"27","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 11","cards":"QT","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 12","cards":"TJ","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 13","cards":"TA","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 14","cards":"J9","wager":"0","bank":"0","handResult":"0"},
			{"name":"Team 15","cards":"26","wager":"0","bank":"0","handResult":"0"}
		]};
    var handhistJSON = [];
	var handhist = {};
	var currentHandId = 0;



	// for (const [key, value] of Object.entries(handdata)) {
	// 	console.log(key, value);
	// }
	var game      = new Game(),
		running   = false,
		blackjack = false,
		insured   = 0,
		deal,
		domLoaded = false;

/*****************************************************************/
/*************************** Classes *****************************/
/*****************************************************************/

	function Player(id_in, name, isdealer, player_pos, handHistDOM, handId, controller) {
		this.wager = 0;
		this.bank = 0;
		this.x = player_pos["x"];
		this.y = player_pos["y"];
		this.name = name;
        this.id = id_in;
        this.isdealer = isdealer;
        this.handId = handId;
		this.handHistDOM = handHistDOM;
		this.controller = controller.trim();
		this.combinedId = `${id_in}_h${handId}`;

		this.addPlayerHTML= function() {
			let infoclass = "info";
			if (this.id < 3){
				infoclass = "rightinfo";
			}

			let playerDOMid = `player_${this.combinedId}`;

			htmlStr =
				`    <div id=\"${playerDOMid}\">\n` +
				`      <div id=\"state_${this.combinedId}\" class=\"bj_pstate\"></div>\n` +
				`      <div id=\"info_${this.combinedId}\" class=\"info\">\n` +
				`        <span id=\"name_${this.combinedId}\" class=\"name\"><span></span></span>\n` +
				`        <span id=\"hand_${this.combinedId}\" class=\"hand\"><span></span></span>\n` +
				`        <div id=\"bank_${this.combinedId}\" class=\"bank\"><span></span></div>\n` +
				`      </div>\n` +
				`      <div id=\"phand_${this.combinedId}\" class=\"phand\"></div>\n` +
				`    </div>`;

			this.handHistDOM.append(htmlStr);

			this.playerDOM = this.handHistDOM.find(`#${playerDOMid}`);
			if (this.controller.length > 0){
			    this.playerDOM.find(".name span").html(`${this.name} <span class="small">(${this.controller})</span>`);
			}
			this.infoDOM = this.playerDOM.find('.info');
			this.stateele = this.playerDOM.find('.bj_pstate');
			if (this.id <= 5 || this.id >= 12){ // right side info
				this.infoDOM.css({top:this.y-90, right: this.x-175});
				this.stateele.css({top:this.y-115, right: this.x-175});
			} else {
				this.infoDOM.css({top:this.y+20, right: this.x});
				this.stateele.css({top:this.y-125, right: this.x});
			}
            if (isdealer){

				//this.stateele.css({top:this.y+20, right: this.x, 'font-size': '18px'});

			} else {
            	this.renderBet();
				this.renderBankValue();
			}

			//player.showName();
		};
		this.renderLost = function(){
			this.stateele.css({'color':'red'});
			this.stateele.html(`Lost: -${this.getWager()}`)
		}
		this.renderWon = function(){
			this.stateele.css({'color':'green'});
			this.stateele.html(`Won: ${this.getWager()}`)
		}
		this.renderPush = function(){
			this.stateele.css({'color':'blue'});
			this.stateele.html(`Push`)
		};

		this.renderReset = function(){
			// let handDOM = this.playerDOM.find(`#phand_${this.id}`);
			// handDOM.stop(false, false);
			// handDOM.find('.up').each(function(index, ele){
			// 	$(ele).stop(true, true);
			// 	console.log("stopping->", ele.id);
			// });
			//this.playerDOM.remove();
			// let game = $("#game");
			// game.html('');
		};
		this.renderBet = function(){
			if (this.wager > 0){
				this.stateele.css({'color':'white'});
				this.stateele.html(`Bet: ${this.getWager()}`)
			} else {
				this.stateele.css({'color':'#444444'});
				this.stateele.html(`No Bet`)
			}

		};
        this.renderCard = function (showcard=false) {
			let card = this.unshownCards.shift();
			if (card == null){
				console.log("No cards to be found, skipping ");
				return;
			}
			let cardnum = this.shownCards.length;
			let cardXPos = this.x - 40 * cardnum;

			let speed = 500;

			//this.playerDOM.find('.hand span').html("Card Total: " + this.getScore());
			let img = `${card.getRank()}_of_${card.getStringSuit()}.png`;
			let type = "up";

			let handDOM = this.playerDOM.find(`#phand_${this.combinedId}`);
			let cardid = `card-${this.combinedId}-${cardnum}`;
			let html = "";
			if (this.isdealer && cardnum === 1 && !showcard){

				html =
				`<div id="${cardid}" class="${cardid} down">` +
				'    <span class="pos-0">' +
				`        <img width="75px" src="cards/cardback.png" class="cardback" />` +
				`        <img width="75px" src="cards/${img}" class="card_down" />` +
				'    </span>' +
				'</div>';
			} else {

				html =
				`<div id="${cardid}" class="${cardid} up">` +
				'    <span class="pos-0">' +
				`        <img width="75px" src="cards/${img}" />` +
				'    </span>' +
				'</div>';
			}
			this.shownCards.push(card);

			handDOM.append(html);

			let cardDOM = handDOM.find(`#${cardid}`);
			//var sheet = window.document.styleSheets[0];
			if (this.handId > 0){
				cardDOM.css({'top': "", 'right':``});
			}

			// cardDOM.css('right',cardXPos);

			cardDOM.animate({
				'top': this.y,
				'right': cardXPos
			}, speed);
			//
			handDOM.queue(function() {
				$(this).animate({ 'left': '-=25.5px' }, 100);
				$(this).dequeue();
			});

		};
		this.won = function(){
			this.addToBank(this.wager);
			this.renderWon();
		};
		this.lost = function(){
			this.addToBank(this.wager * -1);
			this.renderLost();
		};
		this.push = function(){
			this.renderPush();
		};
		this.setHandId = function (handId){
			this.handId = handId;
		};

		this.determineResult = function (dealer) {
			var phand    = this.getHand(),
				dhand    = dealer.getHand(),
				pscore   = this.getScore(),
				dscore   = dealer.getScore(),
				wager    = this.getWager(),
				winnings = 0,
				result;

			running = false;
            //console.log(this.id, dhand, dscore, phand, pscore, wager);
			if(pscore > dscore) {
				if(pscore === 21 && phand.length < 3) {
					winnings = (wager * 2);// + (wager / 2);
					this.won()
					//result = 'Blackjack!';
				} else if(pscore <= 21) {
					this.won()
					//result = 'You win!';
				} else if(pscore > 21) {
					this.lost()
					result = 'Bust';
				}
			} else if(pscore < dscore) {
				if (pscore > 21){
					this.lost();
					//result = 'Bust';
				} else if(dscore > 21) {
					this.won();
					//result = 'You win - dealer bust!';
				} else if(dscore <= 21) {
					this.lost()
					//result = 'You lose!';
				}
			} else if(pscore === dscore) {
				if(pscore <= 21) {
					if(dscore === 21 && dhand.length < 3 && phand.length > 2) {
						this.lost()
						//result = 'You lose - dealer Blackjack!';
					} else {
						this.push()
						//result = 'Push';
					}
				} else {
					this.lost()
					//result = 'Bust';
				}
			}
			this.renderBankValue();
		};

        this.renderScore = function (){
			let scoreDOM = this.playerDOM.find(`.hand span`);
			scoreDOM.html(`Score: ${this.getScore()}`);
		};

        this.setHand = function(cards){
        	this.unshownCards = [];
        	this.shownCards = [];
			for (var i = 0; i < cards.length; i++) {
				this.unshownCards.push(new Card(cards.charAt(i)));
			}
		};
		this.getHand = function() {
			return this.shownCards;
		};

		this.resetHand = function() {
			this.shownCards = [];
			this.unshownCards = [];
		};

		this.getWager = function() {
			return this.wager;
		};

		this.setWager = function(money) {
			this.wager = parseInt(money, 0);
		};

		this.resetWager = function() {
			this.wager = 0;
		};

		this.renderBankValue = function() {
			if (!this.isdealer){
				bankele = $(`#bank_${this.combinedId}`);
				bankele.html('Money: $' + this.bank)//.formatMoney(2, '.', ','));

				if(this.bank < 0) {
					bankele.html('Winnings: <span style="color: #D90000">-$' +
						bank.toString().replace('-', '') + '</span>'); //.formatMoney(2, '.', ',')
				}
			}

		};

		this.getBank = function() {
			return this.bank;
		};

		this.setBank = function(money) {
			this.bank = money;
		};

		this.addToBank = function(money) {
			this.bank += money;
		};

		this.flipCards = function() {
			var foundDown = false;
			$('.down').each(function() {
				//$(this).remove();
				thiz = $(this)
				thiz.addClass("up");
				thiz.removeClass("down");
				thiz.find(".cardback").remove()
				thiz.find(".card_down").removeClass("card_down")
				foundDown = true;
			});
			// if (foundDown){
			// 	this.renderCard(true);
			// }

		};
	}

	function Card(card, suit) {
		var ranks = ['A', '2', '3', '4', '5', '6', '7', '8', '9', '10', 'J', 'Q', 'K'],
			suits = ['&#9824;', '&#9827;', '&#9829;', '&#9670;'];

		this.suitval = suit == null ? Math.floor(Math.random() * 4): suit;
		//console.log(card, ranks.find(element => element === card.toUpperCase()));
		if (card === "T"){
			card = "10";
		}
		this.card = {rank: ranks.find(element => element === card.toUpperCase()),
		             suit: suits[this.suitval]};
		this.getRank = function() {
			return this.card.rank;
		};

		this.getSuit = function() {
			return this.card.suit;
		};

		this.getValue = function() {
			var rank  = this.getRank(),
				  value = 0;

			if(rank === 'A') {
				value = 11;
			} else if(rank === 'K') {
				value = 10;
			} else if(rank === 'Q') {
				value = 10;
			} else if(rank === 'J') {
				value = 10;
			} else {
				value = parseInt(rank, 0);
			}

			return value;
		};
		this.getStringSuit = function(){

			switch (this.suitval){
				case 0:
					return 'spades';
				case 1:
					return 'clubs';
				case 2:
					return 'hearts';
				case 3:
					return 'diamonds';
			}
		}

	}



	function Game() {
		this.newGame = function() {
			var wager = 100

			player.resetWager();
			player.setWager(wager);

			if(player.checkWager()) {

				resetBoard();
				player.setCash(-wager);

				deal      = new Deal();
				running   = true;
				blackjack = false;
				insured   = false;

				player.resetHand();
				dealer.resetHand();
				showBoard();
			} else {
				player.setWager(-state);
				$('#alert').removeClass('alert-info alert-success').addClass('alert-error');
				showAlert('Wager cannot exceed available cash!');
			}
		};
	}

/*****************************************************************/
/************************* Extensions ****************************/
/*****************************************************************/

	Player.prototype.getScore = function() {
		var hand  = this.getHand(),
				score = 0,
				aces  = 0,
				i;

		for(i = 0; i < hand.length; i++) {
			score += hand[i].getValue();

			if(hand[i].getValue() === 11) { aces += 1; }

			if(score > 21 && aces > 0) {
				score -= 10;
				aces--;
			}
		}

		return score;
	};

/*****************************************************************/
/************************** Functions ****************************/
/*****************************************************************/

/*****************************************************************/
/*************************** Loading *****************************/
/*****************************************************************/
	function updateAutoPlayDiv(){
		if (autoplay){
		    $("#autoplayinfo").text("Auto play is enabled, click a button or the OOO logo to disable.");
		} else {
			$("#autoplayinfo").text("Auto play is disabled, click the OOO logo to enable.");
		}

	}
    function switchHand(){
    	//console.log(this);
    	autoplay = false;
    	updateAutoPlayDiv();
    	let buttonId = this.id.replace("hand","");
    	showHand(buttonId);
	}
	function showHand(buttonId){
    	let handId = buttonId - 1;

		$(".handhist").each(function(index, ele){
			if (ele.id === `handhist-${handId}`){
				$(ele).show()
			} else {
				$(ele).hide();
			}
		});

		$(".btn").each(function(index, ele) {
		    $(ele).css({"opacity":"1", "pointer-events": "auto"});
		});
		$(`#hand${buttonId}`).css({"opacity":".5", "pointer-events": "none"});

		if ($(`#handhist-${handId}`).length === 0){
			loadData(handhistJSON, handId);
		}
	}
	async function ready(){
		var timeout = 200;
        var all_cards_thrown = false, actions_complete=false;
		let players = handhist[currentHandId];
		inmotion = true;
		players.forEach(function (player) {
			player.addPlayerHTML();
		});
		$("#game").on("click",function(){

			autoplay = (!autoplay);

			updateAutoPlayDiv();

			if (autoplay){
			    if (!inmotion){
			    	showHand(currentHandId+1);
				}
			}

			return true;
		});
		// show first 2 cards
		var lasttimeout;
		for (let i=0; i < 2; i++) {
			// dealer is last dealt but happens to be first in array.
			for (let pindex = 1; pindex <= players.length; pindex++) {
				setTimeout(function () {
						players[pindex % players.length].renderCard();
						players[pindex % players.length].renderScore();
						if (pindex === players.length && i === 1){
							all_cards_thrown = true;
						}

					}, timeout);
				timeout += 200;
			}
		}

		timeout = 200;
		while (!all_cards_thrown){
			await new Promise(r=>setTimeout(r,500));
		}

		// show player actions
		players_with_hits = []
		for (let pindex =1; pindex < players.length; pindex++) {
			let player = players[pindex];
			//console.log("shown=",player.shownCards.length, "unshown=", player.unshownCards.length);

			for (let hitcnt =0; hitcnt < player.unshownCards.length; hitcnt++){
				players_with_hits.push(pindex);
			}
		}
		all_cards_thrown = false;
		timeout = 0;

		// calc up the hits first, then set timeout so can await last one.
		for (let pw_index=0;pw_index < players_with_hits.length; pw_index++){
			let pindex = players_with_hits[pw_index];
			let render = function () {
						players[pindex % players.length].renderCard();
						players[pindex % players.length].renderScore();
						if (pw_index === (players_with_hits.length -1)){
						    all_cards_thrown = true;
						}
					}
			setTimeout(render, timeout);
			timeout += 200;
		}
		if (players_with_hits.length === 0){
			all_cards_thrown = true;
		}

		// wait for all cards to be thrown before moving on.
		while (!all_cards_thrown){
			await new Promise(r=>setTimeout(r,500));
		}
		timeout = 200;
		// dealer actions
		let dealer = players[0];
		var innerTimeout = 0;
		await dealer.flipCards();
        await new Promise(r=>setTimeout(r,400));

		let player = players[0];
		let cards_to_hit = player.unshownCards.length
		for (let hitcnt =0; hitcnt < cards_to_hit; hitcnt++){
			await dealer.renderCard();
			await dealer.renderScore();
			await new Promise(r=>setTimeout(r,1000));
		}

		innerTimeout += 300;
		// the showdown
		for (let pindex = 1; pindex < players.length; pindex++) {
			let player = players[pindex];
			if (player.getWager() > 0){
				await player.determineResult(dealer);
				await new Promise(r=>setTimeout(r,200));
			}

		}

		await new Promise(r=>setTimeout(r,4000));
		console.log("autolaunch", autoplay, currentHandId < handdata.length, currentHandId, handdata, handhist, handhistJSON,"too many secrets");
		if (autoplay && currentHandId < handdata["players"].length){
			showHand(currentHandId+2);
		}
		inmotion = false


	}

	document.addEventListener("DOMContentLoaded", function() {
		domLoaded = true
	});

	function jsonLoadData(handdata){
		loadData(handdata, -1);
	}
	function renderHandButtons(){
		let buttons = $("#buttons");
		for (let handnum =1; handnum <= handhistJSON.length; handnum++){

			if (buttons.find(`#hand${handnum}`).length === 0){
				buttons.append(`<div id="hand${handnum}" class="btn">Hand ${handnum}</div>`);
				$(`#hand${handnum}`).on( "click", switchHand);
			} else {
				$(`#hand${handnum}`).css({"opacity":"1", "pointer-events": "auto"});
			}
		}
		$(`#hand${currentHandId+1}`).css({"opacity":".5", "pointer-events": "none"});
	}
	function refreshJSON(){
		$.getJSON("./results/index.json", function(indexdata){
			loadIndex(indexdata, -1);
			//$.getJSON(`./results/tick${tick}.json`, jsonLoadData);
		});

	}
	function loadIndex(indexdata, selectedTickId){
		let minTick = indexdata.minTick;
		let maxTick = indexdata.maxTick;
		if (selectedTickId == null){
			selectedTickId = maxTick;
		}

		let tb = $("#tickbox");
		let selected = "";
		let addedTick = false;

		let rounds = [];
		for (let i=0;i< indexdata.rounds.length;i++){
			rounds.push(parseInt(indexdata.rounds[i],10));
		}

		rounds.sort(function(a, b) {
		  if (a > b) return 1;
		  if (b > a) return -1;

		  return 0;
		});

		for (let round_id of rounds){
			//console.log((i), maxTick, (i)=== maxTick);
			if (tb.find(`#topt_${round_id}`).length === 0){
				if (round_id === parseInt(selectedTickId)){
					selected = " selected ";
				}
				addedTick = true;
				tb.append(`<option ${selected} value="${round_id}" id="topt_${round_id}">Round #${round_id}</option>`);
			}
		}

		// animate for arrival of new info
		if (addedTick && selectedTickId === -1){
			tb.addClass("wiggle_notify");
			$("#plus1").show();
			setTimeout(function(){
				tb.removeClass("wiggle_notify");
			}, 1000);
			setTimeout(function(){
				$("#plus1").hide();
			}, 9100);
		}
		return selectedTickId;

	}
	function loadData (handdata, handid=0) {

		handhistJSON = handdata;

		let loadingHand = [];
		let pid = 0;
		if (handid === -1){
			currentHandId = 0; //handdata.length-1;
		} else {
			currentHandId = handid;
		}
		if (handhist[currentHandId] == null){
			handhist[currentHandId] = [];
			let players = handhist[currentHandId];
			let game = $("#game");
			let handHistId = `handhist-${currentHandId}`;
			game.append(`<div id="${handHistId}" class="handhist"></div>`);
			let handhistDOM = game.find(`#${handHistId}`);
			console.log("currentHandId=",currentHandId);
			for (let p of handdata[currentHandId].hand) {
				let player = new Player(pid, p.name, pid === 0, player_pos[pid], handhistDOM, currentHandId, p.controller);
				player.setHand(p.cards);
				if (typeof p.wager === 'string') {
					p.wager = parseInt(p.wager);
				}
				player.setWager(p.wager);
				if (typeof p.bank === 'string') {
					p.bank = parseInt(p.bank);
				}
				// reseting bank b/c sends final result
				if (p.handResult === "1") { // lost
					player.setBank(p.bank + p.wager);
				} else if (p.handResult === "4") { // won
					player.setBank(p.bank - p.wager);
				} else if (p.handResult === "2") { // push
					player.setBank(p.bank);
				} else {
					player.setBank(p.bank + p.wager); // lost
				}
				player.setHandId(handid);
				players.push(player);
				pid++;
			}
		}

		renderHandButtons();

		if (domLoaded) {

			ready();
		} else {
			document.addEventListener("DOMContentLoaded", ready);
		}
	}

	const urlParams = new URLSearchParams(window.location.search);
	const tick = urlParams.get('tick');

	$.getJSON("./results/index.json", function(indexdata){
		let selectedTickId = loadIndex(indexdata, tick);
		let remoteFP = `./results/all-hands-round-${selectedTickId}.json`;
		$.getJSON(remoteFP, jsonLoadData);
	});

	setInterval(refreshJSON, 15000);


	(function($) {
		$(document).ready( function() {
			$('#tickbox').change(function(chosenEle) {
				$(".handhist").each(function(index, ele){
					$(ele).remove();
				});
				handhistJSON = [];
				handhist = {};
				currentHandId = 0;
				//console.log("selected tick is ", this.options[chosenEle.target.selectedIndex].value)
				let selectedTickId = this.options[chosenEle.target.selectedIndex].value
				let remoteFP = `./results/all-hands-round-${selectedTickId}.json`;
				$.getJSON(remoteFP, jsonLoadData);
			});
		});
	})(jQuery);



	//window.onload = ready;

}());