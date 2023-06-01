(function (window, document) {
   // we fetch the elements each time because docusaurus removes the previous
   // element references on page navigation
   function getElements() {
       return {
           layout: document.getElementById('layout'),
           menu: document.getElementById('menu'),
           menuLink: document.getElementById('menuLink')
       };
   }

   function toggleClass(element, className) {
       var classes = element.className.split(/\s+/);
       var length = classes.length;
       var i = 0;

       for (; i < length; i++) {
           if (classes[i] === className) {
               classes.splice(i, 1);
               break;
           }
       }
       // The className is not found
       if (length === classes.length) {
           classes.push(className);
       }

       element.className = classes.join(' ');
   }

   function toggleAll() {
       var active = 'active';
       var elements = getElements();

       toggleClass(elements.layout, active);
       toggleClass(elements.menu, active);
       toggleClass(elements.menuLink, active);
   }
   
   function handleEvent(e) {
       var elements = getElements();
       
       if (e.target.id === elements.menuLink.id) {
           toggleAll();
           e.preventDefault();
       } else if (elements.menu.className.indexOf('active') !== -1) {
           toggleAll();
       }
   }
   
   document.addEventListener('click', handleEvent);

}(this, this.document));

const handlelightSearch = function(input, elements, options) {
   return function (event) {
     let result = 0;
 
     for (const element of elements) {
       if (element.innerHTML.toLowerCase().includes(input.value.toLowerCase())) {
           element.style.display = options.display ? options.display : 'block';
           result++;
 
           if(options.onclick) {
             element.addEventListener('click', options.onclick);
           }
 
       } else {
           element.style.display = options.hide ? options.hide : 'none';
       }
     }
 
     if(options.displayontype) {
       if(input.value.length >= options.displayontype) {
         options.parent.style.display = options.displayparent ? options.displayparent : 'block';
       } else {
         options.parent.style.display = options.hideparent ? options.hideparent : 'none';
       }
     }
 
     if (options.noresultsmsg && options.parent) {
       const errormsg = document.querySelector('.light-search-noresults');
 
       if (!result) {
         const msgtag = options.msgtag ? options.msgtag : 'p';
         const msgtext = options.msgtext ? options.msgtext : 'No results found';
 
         const msgelement = document.createElement(msgtag);
               msgelement.innerHTML = msgtext;
               msgelement.classList.add('light-search-noresults');
 
         if (!errormsg) {
           options.parent.appendChild(msgelement);
         }
       } else {
         if (errormsg) {
           errormsg.remove();
         }
       }
     }
   }
 };
 
 const lightSearch = (input, elements, options = {}) => {
   input.addEventListener('input', handlelightSearch(input, elements, options));
 }
 
 const removelightSearch = (input) => {
   input.removeEventListener('input', handlelightSearch);
 }