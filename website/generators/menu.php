<?php
$menus = [
   'Home' => ['href' => 'index.html', 'icon' => 'typcn-home'],
   'Setup' => ['href' => 'setup.html', 'icon' => 'typcn-cog'],
   'Advanced' =>  ['submenu' => true],
   'WiFi' =>  ['href' => 'wifi.html', 'icon' => 'typcn-wi-fi'],
   'System' =>  ['href' => 'sistema.html', 'icon' => ''],
]
?>
<?php
foreach ($menus as $page => $pkeys) {
   if (array_key_exists('submenu', $pkeys)) continue;
?>

<!-- <?php echo $pkeys['href']; ?> -->
   <div id="menu">
      <div class="pure-menu">
         <a class="pure-menu-heading">MENU</a>
         <ul class="pure-menu-list">
<?php
               foreach ($menus as $menu => $keys) {
                  if (array_key_exists('submenu', $keys)) {
?>
               <li class="pure-menu-item menu-item-divided pure-menu-selected">
                  <a href="#" class="pure-menu-link"><?php echo $menu;?></a>
               </li>
<?php
                  } else
                  {
?>
               <li class="pure-menu-item<?php echo $page==$menu?" pure-menu-active":"";?>"><a href="<?php echo $keys['href'];?>" class="pure-menu-link"><i class="typcn <?php echo $keys['icon'];?>"></i> <?php echo $menu;?></a></li>
<?php 
                  }
               }
?>
         </ul>
      </div>
   </div>


<?php
}
?>